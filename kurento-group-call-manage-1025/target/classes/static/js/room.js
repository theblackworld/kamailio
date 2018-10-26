/*
 * (C) Copyright 2014 Kurento (http://kurento.org/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

var ws = new WebSocket('wss://' + location.host + '/roomManagement');
var participants = {};
var name;


window.onbeforeunload = function() {//回调函数，页面关闭时调用，关闭ws
	ws.close();
};

ws.onmessage = function (message) {
    console.info('Received message: ' + message.data);
    if (message.data == "refresh")
        //window.location.replace('https://' + location.host + '/participants');
        var form1 = document.getElementById('test_form');
    	form1.submit();
}

ws.onopen = function (ev) {
    console.info('pingcoool ws opened' + location.host);
    ws.send("pingcoool_send ws message!");
}



function sendMessage(message) {
	var jsonMessage = JSON.stringify(message);
	console.log('Senging message: ' + jsonMessage);
	ws.send(jsonMessage);
}
