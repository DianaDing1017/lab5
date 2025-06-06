import requests
import json
import numpy as np

# URL of your running Flask app
url = "http://172.20.20.20:8000/predict"  # Change to your URL

# Replace FEATURE_SIZE with your actual input length, e.g., 300
FEATURE_SIZE = 300
dummy_data = np.random.rand(FEATURE_SIZE).tolist()
print("Data shape:", len(dummy_data))  # should print 300

# Build the JSON payload
payload = {
    "data": dummy_data
}

# Send the POST request
headers = {"Content-Type": "application/json"}
response = requests.post(url, data=json.dumps(payload), headers=headers)

# Handle response
if response.status_code == 200:
    print("Prediction received:")
    print(response.json())
else:
    print("Error:")
    print(f"Status code: {response.status_code}")
    print(response.text)
