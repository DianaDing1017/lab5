# TECHIN515 Lab 5 - Edge-Cloud Offloading

This project builds upon lab 4. We will implement an edge-cloud offloading strategy, which performs inference or computation locally on ESP32, and offloads it to the cloud (Microsoft Azure) under certain conditions.

The outline of this lab is as follows:

- Train and host a model in Azure
- Deploy the model endpoint as a web app
- Configure magic wand to offload to cloud when uncertain

## Learning Objectives

By completing this lab, students will:

- Be capable of training and hosting models in Azure
- Deploy model as a web app
- Understand cloud-edge offloading

## Hardware Requirements

- Your magic wand from Lab 4

## Software Requirements

### Arduino IDE

- Arduino IDE with ESP32 board support
- Required libraries:
  - Adafruit MPU6050
  - Adafruit Sensor
  - Wire (built-in)

### Microsoft Azure account

### Gesture dataset collected in previous labs

### Python (for data capture)

- Python 3.8 or newer
- Required packages (install via `pip install -r requirements.txt`)

## Getting Started

### 1. Microsoft Azure: Resource Group

1. Navigate to [Microsoft Azure](https://azure.microsoft.com/en-us/) and sign in with your account. If you are new to Azure, you should get $200 free trial credit. 
2. Click on `Resource groups` as shown below to create a resource group.

<img src="assets/resource_group.png" width="800">

3. Name your resource group, e.g., TECHIN515-lab, and choose the closet region, e.g., West US 2. Click `Review+Create`, then `Create` to create the resource group. You can use the default subscription "Azure subscription 1".
   - You would like to have the region close to your location to reduce latency.

### 2. Microsoft Azure: Create a Machine Learning Workspace

1. Navigate back to homepage by clicking on Microsoft Azure on top left, or use this [link](https://portal.azure.com).
2. Click on `Azure Machine Learning` as shown below.

<img src="assets/AzureML.png" width="800">

3. Click on `+Create` to create an AzureML workspace.
   - Use default subscription, i.e., Azure subscription 1
   - Attach the workspace to the resource group you just created
   - Name your workspace, e.g., TECHIN515-lab
   - Use the same region as your resource group
   - Leave storage account, key valut, and application insights as deafult
4. Click on `Review+Create`, then `Create` to create the workspace.

### 3. Microsoft Azure: Create a Compute Instance

There are two types of compute in AzureML: Compute instance and compute cluster. The former is used for development and the latter is used for scalable training jobs.
In the following, we will create a compute instance.

1. Go to your ML workspace, launch ML studio, and click on `Compute` in the left sidebar.

<img src="assets/compute.png" width="200">

2. Choose the `Compute Instances` tab, and click `+New`.
3. Fill in name, virtual machine size, and region. Note that the region should match that of your workspace.
4. Click `Create`. It may take a few minutes to create the compute instance.

### 4. Microsoft Azure: Host Data

1. Locate the training data in your laptop. We will host the data in Azure Blob. Note that as the cloud has higher computing performance and more storage, you can merge your training data with that collected by other students to improve your model performance.
2. Go to AzureML and navigate to Data tab. Click `+Create`.
3. Name you data asset along with a brief description. Choose `Folder (uri_folder)` as type. Click `Next`.
4. Choose From local files option, and upload your training dataset. Click `Next`.
5. Leave datastore type as Azure Blob storage and click `Next`.
6. Upload your training dataset folder. Click `Next` and then `Create`.

### 5. Microsoft Azure: Model Training and Register

1. In the Compute tab, click on JupyterLab.
2. Copy the provided code in `trainer_scripts/train.ipynb` to the Jupyter Notebook in Azure. Use *Python3.10-AzureML* as the kernel to run the code. Once the cell completes running, you should see a `h5` file being created.
   - Note that you need to configure the path to training dataset. An example is given below:
     ```py
     dataset = Dataset.File.from_files(path=(ws.get_default_datastore(), 'UI/2025-04-05_231528_UTC/**'))
     ```
   - To find the correct path, click on Data tab on the left sidebar, and then the dataset you created in Blobstore. Click on View in datastores brows to find the path. Please leave `/**` as such and replace the prefix with your relative path.

<img src="assets/data.png" width="800">

3. **You can skip this step if you do not plan to host your ML model on Azure**. Copy the provided code in `model_register.ipynb` to a new Jupyter Notebook in Azure. Use *Python3.10-SDK v2* as the kernel to run the code.
   - Note that you need to configure the following code block in the 3rd cell accordingly:

```py
SUBSCRIPTION = "<subscription_ID>"
RESOURCE_GROUP = "<group_name>"
WS_NAME = "<workspace_name>"
```

To find such information, click the tab on top right showing your subscription and workspace name. Copy Resource Group to `RESOURCE_GROUP`, Subscription ID to `SUBSCIPTION`, and Current workspace to `WS_NAME`.

Once the code is executed successfully, our trained model is registered in Azure Machine Learning and an online endpoint is created. To deploy our model, we need another script (`inference_scripts/score.py`) to define how the model handles incoming requests. The script is provided for your reference in the future. You will not need it in this lab since we will use a local server to mimic the process.

### 6. Deploy Model via a Web App

In general, we do not need to deploy a web app for ML inference. In fact, our model is ready for deployment, and we can direct incoming traffic to it for inference. However, when your client would like to have a front end for your ML model, a web app is then necessary. We will follow this approach for this lab, although the front end looks like a place holder whose main purpose now is to direct traffic.

Navigate to `app` directory. Activate a virtual environment and install the dependencies defined in `requirements.txt`. Then run `app.py` to start the server.

```py
python app.py
```
Once the web app starts, you should see an URL, which will be needed to perform inference on server.

Optionally, you can deploy the web app using Microsoft Azure by navigating to `App Services`. Note that this option is subject to cost, especially when you need to host large ML models.

### 7. Cloud-Edge Offloading

Based on the ESP32 sketch used during wand duel, you should modify it so that your wand performs inference locally when confidence is high, and redirects sensor readings to server when confidence is low.

1. Change serverUrl to your web URL. At the top of your sketch, add the following threshold definition for confidence

   ```cpp
   #define CONFIDENCE_THRESHOLD 80.0
   ```

2. Complete the following code block and add it to your sketch at appropriate place:

   ```cpp
      if (confidence < CONFIDENCE_THRESHOLD) {
            Serial.println("Low confidence - sending raw data to server...");
            sendRawDataToServer();
        } else {
            // add your code to actuate LED based on 
        }
   ```

3. Complete the function `sendRawDataToServer` so that your sensor reading is directed to the server and LED is actuated accordingly. You may use the `sendGestureToServer` function from wand duel as a starting point.

   ```cpp
   void sendRawDataToServer() {
      HTTPClient http;
      http.begin(serverUrl);
      http.addHeader("Content-Type", "application/json");

      // Build JSON array from features[]
      // Your code here

      int httpResponseCode = http.POST(jsonPayload);
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);

      if (httpResponseCode > 0) {
         String response = http.getString();
         Serial.println("Server response: " + response);

         // Parse the JSON response
         DynamicJsonDocument doc(256);
         DeserializationError error = deserializeJson(doc, response);
         if (!error) {
               const char* gesture = doc["gesture"];
               float confidence = doc["confidence"];

               Serial.println("Server Inference Result:");
               Serial.print("Gesture: ");
               Serial.println(gesture);
               Serial.print("Confidence: ");
               Serial.print(confidence);
               Serial.println("%");
               // Your code to acutate LED
         } else {
               Serial.print("Failed to parse server response: ");
               Serial.println(error.c_str());
         }

      } else {
         Serial.printf("Error sending POST: %s\n", http.errorToString(httpResponseCode).c_str());
      }

      http.end();
   }
   ```

4. Tune the confidence interval to control when ESP32 should consult to the cloud for gesture inference.
5. Take a picture your serial monitor for cases when ESP32 performs inference locally, and when ESP32 consults cloud for inference.
6. If your ESP32 keeps performing local inference with high confidence, you can collect one additional class of gestures and train the model in cloud. Then test your magic wand with the new gesture data, which should not be seen before by your wand.
7. **Discussion**:
   1. Is server's confidence always higher than wand's confidence from your observations? What is your hypothetical reason for the observation?
   2. Sketch the data flow of this lab.
   3. Our approach is edge-first, fallback-to-server when uncertain. Analyze pros and cons of this approach from the following aspects: reliance on connectivity, latency, prediction consistency, data privacy. 
   4. Name a strategy to mitigate at least one limitation named in question 3.

### 8. Clean Up Resources (**Important**)

1. If you have completed this lab, and do not need the resources any more, you can go to resource groups from portal menu and delete resource group. This operation may take a few minutes to complete. Clean up resources will help to manage your bills incurred when using cloud services.

## Deliverables

1. GitHub link to your project with detailed README. An example of project structure for your GitHub is given below:

```
.
├── ESP32_to_cloud/             # ESP32 Arduino code
│   └── ESP32_to_cloud.ino      # Main ESP32 sketch
├── trainer_scripts             # Scripts
    ├── train.ipynb                 # Model training script
    ├── model_register.ipynb        # Model register script
├── app/                        # Web app for model deployment
    ├── wand_model.h5               # trained model
    ├── app.py                      # Script of web app
    ├── requirements.txt            # Dependencies required by web app
└── data/                       # Training data directory
    ├── O/                           # O-shape gesture samples
    ├── V/                           # V-shape gesture samples
    ├── Z/                           # Z-shape gesture samples
    └── some_class/                  # Some other gesture samples
```

2. Pictures of serial monitor
3. Report of all discussion questions
