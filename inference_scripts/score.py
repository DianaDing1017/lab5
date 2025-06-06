import json
import numpy as np
import tensorflow as tf
import os

def init():
    global model
    model_path = os.path.join(os.getenv("AZUREML_MODEL_DIR"), "wand_model.h5")
    model = tf.keras.models.load_model(model_path)

def run(raw_data):
    try:
        data = json.loads(raw_data)["data"]
        data = np.array(data)
        preds = model.predict(data)
        pred_classes = np.argmax(preds, axis=1)
        return {"predictions": pred_classes.tolist()}
    except Exception as e:
        return {"error": str(e)}
