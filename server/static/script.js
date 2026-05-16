async function updateStatus(){

    let response = await fetch("/get_status");

    let data = await response.json();

    document.getElementById("status").innerText =
        data.status;
}

setInterval(updateStatus, 1000);

async function uploadFirmware(){

    let fileInput =
        document.getElementById("firmwareFile");

    let file = fileInput.files[0];

    if(!file){
        alert("Select firmware first");
        return;
    }

    let formData = new FormData();

    formData.append("file", file);

    let response = await fetch("/upload", {
        method: "POST",
        body: formData
    });

    let result = await response.json();

    alert(result.message);
}