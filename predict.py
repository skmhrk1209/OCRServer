import tensorflow as tf
import numpy as np
import cv2
import base64
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

    #image = np.frombuffer(base64.decode(base64_encoded), dtype=np.uint8)
    image = cv2.imread(base64_encoded)

    predict_results = classifier.predict(
        input_fn=tf.estimator.inputs.numpy_input_fn(
            x={"image": image[np.newaxis, :, :, :].astype(np.float32) / 255.0},
            batch_size=1,
            num_epochs=1,
            shuffle=False
        )
    )

    class_ids = {}

    for i in range(ord("0"), ord("z") + 1):

        if ord("0") <= i <= ord("9"):
            class_ids[chr(i)] = i - ord("0")
        elif ord("A") <= i <= ord("Z"):
            class_ids[chr(i)] = i - ord("A") + class_ids["9"] + 1
        elif ord("a") <= i <= ord("z"):
            class_ids[chr(i)] = i - ord("a") + class_ids["Z"] + 1

    class_ids[""] = max(class_ids.values()) + 1

    chars = {class_id: char for char, class_id in class_ids.items()}

    prediction = "_".join([
        "".join([chars[class_id] for class_id in prediction])
        for prediction in next(predict_results)["predictions"]
    ])

    return prediction
