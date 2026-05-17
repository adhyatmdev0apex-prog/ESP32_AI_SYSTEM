update_available = False

selected_firmware = "/uploads/firmware.bin"

latest_version = "1.0.0"

from fastapi import FastAPI, Request, UploadFile, File
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates

import shutil
import os

app = FastAPI()

# folders
os.makedirs("uploads/runtime", exist_ok=True)

# static mounts
app.mount("/static", StaticFiles(directory="static"), name="static")
app.mount("/uploads", StaticFiles(directory="uploads"), name="uploads")

templates = Jinja2Templates(directory="templates")

# status
device_status = "OFFLINE"

# firmware path
LATEST_FIRMWARE = "uploads/runtime/firmware.bin"

# firmware version
CURRENT_VERSION = "1.0.0"


@app.get("/", response_class=HTMLResponse)
async def home(request: Request):

    return templates.TemplateResponse(
        request=request,
        name="index.html"
    )


@app.get("/esp32_status")
async def esp32_status(name: str):

    global device_status

    device_status = f"{name} ONLINE"

    print(device_status)

    return {
        "message": "Status received",
        "device": name
    }


@app.get("/get_status")
async def get_status():

    return {
        "status": device_status
    }


# OTA manifest
@app.get("/device_config")
async def device_config():

    global selected_firmware
    global latest_version

    return {
        "project": "manager",
        "version": latest_version,
        "firmware": selected_firmware
    }

# upload check
@app.get("/device_command")
async def device_command():

    global update_available
    global selected_firmware

    response = {
        "update": update_available,
        "firmware": selected_firmware
    }

    update_available = False

    return response


# firmware upload
@app.post("/upload")
async def upload_firmware(file: UploadFile = File(...)):

    global update_available
    global selected_firmware
    global latest_version

    firmware_path = f"uploads/{file.filename}"

    with open(firmware_path, "wb") as buffer:
        shutil.copyfileobj(file.file, buffer)

    selected_firmware = f"/uploads/{file.filename}"

    # Auto increase version
    version_parts = latest_version.split(".")

    patch = int(version_parts[2]) + 1

    latest_version = (
        f"{version_parts[0]}."
        f"{version_parts[1]}."
        f"{patch}"
    )

    update_available = True

    print("Firmware uploaded!")
    print(selected_firmware)

    return {
        "message": "Firmware uploaded successfully",
        "firmware": selected_firmware,
        "version": latest_version
    }