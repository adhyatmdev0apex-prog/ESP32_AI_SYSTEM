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

    return {
        "project": "manager",
        "version": "1.0.0",
        "firmware": "/uploads/firmware.bin"
    }


# firmware upload
@app.post("/upload")
async def upload_firmware(file: UploadFile = File(...)):

    global CURRENT_VERSION

    with open(LATEST_FIRMWARE, "wb") as buffer:
        shutil.copyfileobj(file.file, buffer)

    print("Firmware uploaded!")

    # auto increase version
    parts = CURRENT_VERSION.split(".")

    major = int(parts[0])
    minor = int(parts[1])
    patch = int(parts[2])

    patch += 1

    CURRENT_VERSION = f"{major}.{minor}.{patch}"

    print("NEW VERSION:", CURRENT_VERSION)

    return {
        "message": "Firmware uploaded successfully",
        "version": CURRENT_VERSION
    }
