import tensorflow as tf
import numpy as np
import cv2
import base64
import json
import image as img
from attrdict import AttrDict
from model import Model
from networks.residual_network import ResidualNetwork
from networks.attention_network import AttentionNetwork


def predict(base64_encoded):

    classifier = tf.estimator.Estimator(
        model_fn=Model(
            convolutional_network=ResidualNetwork(
                conv_param=AttrDict(filters=64, kernel_size=[7, 7], strides=[2, 2]),
                pool_param=None,
                residual_params=[
                    AttrDict(filters=64, strides=[2, 2], blocks=2),
                    AttrDict(filters=128, strides=[2, 2], blocks=2),
                ],
                num_classes=None,
                channels_first=False
            ),
            attention_network=AttentionNetwork(
                conv_params=[
                    AttrDict(filters=4, kernel_size=[9, 9], strides=[2, 2]),
                    AttrDict(filters=4, kernel_size=[9, 9], strides=[2, 2]),
                ],
                deconv_params=[
                    AttrDict(filters=16, kernel_size=[3, 3], strides=[2, 2]),
                    AttrDict(filters=16, kernel_size=[3, 3], strides=[2, 2]),
                ],
                rnn_params=[
                    AttrDict(sequence_length=4, num_units=[256]),
                    AttrDict(sequence_length=10, num_units=[256])
                ],
                channels_first=False
            ),
            num_classes=63,
            channels_first=False,
            accuracy_type=Model.AccuracyType.EDIT_DISTANCE,
            hyper_params=AttrDict(attention_map_decay=0.001)
        ),
        model_dir="multi_synth_acnn_model"
    )

    image = cv2.imdecode(np.frombuffer(base64.b64decode(base64_encoded), dtype=np.uint8), cv2.IMREAD_COLOR)
    image = image.reshape(256, 256, 3).astype(np.float32) / 255.0

    predict_result = next(classifier.predict(
        input_fn=tf.estimator.inputs.numpy_input_fn(
            x={"image": image[np.newaxis, :, :, :]},
            batch_size=1,
            num_epochs=1,
            shuffle=False
        )
    ))

    class_ids = {}
    class_ids.update({chr(j): i for i, j in enumerate(range(ord("0"), ord("9") + 1), 0)})
    class_ids.update({chr(j): i for i, j in enumerate(range(ord("A"), ord("Z") + 1), class_ids["9"] + 1)})
    class_ids.update({chr(j): i for i, j in enumerate(range(ord("a"), ord("z") + 1), class_ids["Z"] + 1)}),
    class_ids.update({"": max(class_ids.values()) + 1})

    class_chars = dict(map(lambda key_value: key_value[::-1], class_ids.items()))

    predictions = ["".join([class_chars[class_id] for class_id in class_ids]) for class_ids in predict_result["predictions"]]

    bounding_boxes = [[
        img.search_bounding_box(img.scale(attention_map, attention_map.min(), attention_map.max(), 0.0, 1.0), 0.5)
        for class_id, attention_map in zip(class_ids, attention_maps) if class_chars[class_id]
    ] for class_ids, attention_maps in zip(predict_result["predictions"], predict_result["attention_maps"])]

    return json.dumps(dict(predictions=predictions, bounding_boxes=bounding_boxes))
