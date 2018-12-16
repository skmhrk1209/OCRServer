import os
import tensorflow as tf
import tensorflow_hub as hub
import numpy as np
import base64
import json
import cv2
import sys

classifier = hub.Module("https://tfhub.dev/google/imagenet/resnet_v2_152/classification/1")
generator = hub.Module("https://tfhub.dev/deepmind/biggan-512/2")


def classify(image):

    x = tf.placeholder(tf.float32, [None, 224, 224, 3])
    y = tf.argmax(classifier(x), axis=-1)

    with tf.Session() as session:

        session.run(tf.global_variables_initializer())
        return session.run(y, feed_dict={x: [image]})[0]


def generate(class_id):

    y = tf.placeholder(tf.int32, [None])
    z = tf.truncated_normal([tf.shape(y)[0], 128])
    x = generator(dict(y=tf.one_hot(y, 1000), z=z, truncation=1.0))

    with tf.Session() as session:

        session.run(tf.global_variables_initializer())
        return session.run(x, feed_dict={y: [class_id]})[0]


def process(json_encoded):

    json_decoded = json.loads(json_encoded)

    if json_decoded["process_type"] == "classification":

        image = base64.b64decode(json_decoded["image"])
        image = np.fromstring(image, np.uint8)
        image = cv2.imdecode(image, cv2.IMREAD_COLOR)
        image = (image / 255.0).astype(np.float32)

        class_id = classify(image)

        return json.dumps(dict(class_id=class_id))

    elif json_decoded["process_type"] == "generation":

        image = generate(json_decoded["class_id"])

        image = ((image * 0.5 + 0.5) * 255.0).astype(np.uint8)
        image = cv2.imencode(".png", image)[1]
        image = image.tostring()
        image = base64.b64encode(image)

        return json.dumps(dict(image=image))


if __name__ == "__main__":

    print(process(raw_input()))
