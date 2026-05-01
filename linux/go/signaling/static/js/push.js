'use strict';

var localVideo = document.getElementById("localVideo");
var pushBtn = document.getElementById("pushBtn");
var stopPushBtn = document.getElementById("stopPushBtn");

pushBtn.addEventListener("click", startPush);
stopPushBtn.addEventListener("click", stopPush);

var uid = $("#uid").val();
var streamName = $("#streamName").val();
var audio = $("#audio").val();
var video = $("#video").val();
var isDtls = $("#isDtls").val();
var offer = "";
var pc;
const config = {};
var localStream;
var lastConnectionState = "";

function startPush() {
    console.log("send push: /signaling/push");

    $.post("/signaling/push",
        {"uid": uid, "streamName": streamName, "audio": audio, "video": video, "isDtls": isDtls},
        function(data, textStatus) {
            console.log("push response: " + JSON.stringify(data));
            if ("success" == textStatus && 0 == data.errNo) {
                $("#tips1").html("<font color='blue'>推流请求成功!</font>");
                console.log("offer sdp: \n" + data.data.sdp);
                offer = data.data;
                pushStream();
            } else {
                $("#tips1").html("<font color='red'>推流请求失败!</font>");
            }
        },
        "json"
    );
}

function stopPush() {
    console.log("send stop push: /signaling/stoppush");

    localVideo.srcObject = null;
    if (localStream && localStream.getAudioTracks()) {
        localStream.getAudioTracks()[0].stop();
    }

    if (localStream && localStream.getVideoTracks()) {
        localStream.getVideoTracks()[0].stop();
    }

    if (pc) {
        pc.close();
        pc = null;
    }

    $("#tips1").html("");
    $("#tips2").html("");
    $("#tips3").html("");

    $.post("/signaling/stoppush",
        {"uid": uid, "streamName": streamName},
        function(data, textStatus) {
            console.log("stop push response: " + JSON.stringify(data));
            if ("success" == textStatus && 0 == data.errNo) {
                $("#tips1").html("<font color='blue'>停止推流请求成功!</font>");
            } else {
                $("#tips1").html("<font color='red'>停止推流请求失败!</font>");
            }
        },
        "json"
    );

}

function sendAnswer(answerSdp) {
    console.log("send answer: /signaling/sendanswer");

    $.post("/signaling/sendanswer",
        {"uid": uid, "streamName": streamName, "answer": answerSdp, "type": "push"},
        function(data, textStatus) {
            console.log("send answer response: " + JSON.stringify(data));
            if ("success" == textStatus && 0 == data.errNo) {
                $("#tips3").html("<font color='blue'>answer发送成功!</font>");
            } else {
                $("#tips3").html("<font color='red'>answer发送失败!</font>");
            }
        },
        "json"
    );
}

function pushStream() {
    pc = new RTCPeerConnection(config);
    pc.oniceconnectionstatechange = function(e) {
        var state = "";
        if (lastConnectionState != "") {
            state = lastConnectionState + "->" + pc.iceConnectionState;
        } else {
            state = pc.iceConnectionState;
        }

        $("#tips2").html("连接状态: " + state);
        lastConnectionState = pc.iceConnectionState;
    }
    
    pc.setRemoteDescription(offer).then(
        setRemoteDescriptionSuccess,
        setRemoteDescriptionError
    );
}

window.addEventListener("message", function(event) {
    if (event.origin != window.location.origin) {
        return;
    }

    if (event.data.type) {
        if (event.data.type == "SS_DIALOG_SUCCESS") {
            console.log("用户同意屏幕共享, streamId: " + event.data.streamId);
            startScreenStreamFrom(event.data.streamId);
        } else if (event.data.type == "SS_DIALOG_CANCEL") {
            console.log("用户取消屏幕共享");
        }
    }
});

function startScreenStreamFrom(streamId) {
    var constraints = {
        audio: false,
        video: {
            mandatory: {
                chromeMediaSource: "desktop",
                chromeMediaSourceId: streamId,
                maxWidth: window.screen.width,
                maxHeight: window.screen.height
            }
        }
    };

    navigator.mediaDevices.getUserMedia(constraints).then(
        handleSuccess).catch(handleError);
}

function handleSuccess(stream) {
    navigator.mediaDevices.getUserMedia({audio: true}).then(
        function(audioStream) {
            stream.addTrack(audioStream.getAudioTracks()[0]);
            localVideo.srcObject = stream;
            localStream = stream;
            pc.addStream(stream);
            pc.createAnswer().then(
                createSessionDescriptionSuccess,
                createSessionDescriptionError               
            );
        }
    ).catch(handleError);
}

function handleError(error) {
    console.log("get user media error: " + error);
}

function setRemoteDescriptionSuccess() {
    console.log("pc set remote description success");
    console.log("request screen share");
    // 弃用屏幕共享插件
    // window.postMessage({type: "SS_UI_REQUEST", text: "push"}, "*");
    // 无需安装插件
    var constraints = {
        audio: false,
	video: true,
    };
    navigator.mediaDevices.getDisplayMedia(constraints).then(
        handleSuccess).catch(handleError);
}

function createSessionDescriptionSuccess(answer) {
    console.log("answer sdp: \n" + answer.sdp);
    console.log("pc set local sdp");
    pc.setLocalDescription(answer).then(
        setLocalDescriptionSuccess,
        setLocalDescriptionError
    );

    sendAnswer(answer.sdp);
}

function setLocalDescriptionSuccess() {
    console.log("set local description success");
}

function setRemoteDescriptionError(error) {
    console.log("pc set remote description error: " + error);
}

function setLocalDescriptionError(error) {
    console.log("pc set local description error: " + error);
}

function createSessionDescriptionError(error) {
    console.log("pc create answer error: " + error);
}


