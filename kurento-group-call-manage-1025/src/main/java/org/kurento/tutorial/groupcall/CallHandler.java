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

package org.kurento.tutorial.groupcall;

import java.io.IOException;

import org.kurento.client.IceCandidate;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.web.socket.CloseStatus;
import org.springframework.web.socket.TextMessage;
import org.springframework.web.socket.WebSocketSession;
import org.springframework.web.socket.handler.TextWebSocketHandler;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonObject;

/**
 * 
 * @author Ivan Gracia (izanmail@gmail.com)
 * @since 4.3.1
 */
public class CallHandler extends TextWebSocketHandler {
    public CallHandler() {
        System.out.println("CallHandler");
    }

    private static final Logger log = LoggerFactory.getLogger(CallHandler.class);

  private static final Gson gson = new GsonBuilder().create();


  @Autowired
  //告诉springboot，该类依赖下面的类，springboot需要将依赖注入到这里
  private RoomManager roomManager;

  @Autowired
  private UserRegistry registry;

  @Override
  public void handleTextMessage(WebSocketSession session, TextMessage message) throws Exception {
    final JsonObject jsonMessage = gson.fromJson(message.getPayload(), JsonObject.class);

    final UserSession user = registry.getBySession(session);

    if (user != null) {
      log.debug("Incoming message from user '{}': {}", user.getName(), jsonMessage);
    } else {
      log.debug("Incoming message from new user: {}", jsonMessage);
    }

    switch (jsonMessage.get("id").getAsString()) {
      case "joinRoom":
        joinRoom(jsonMessage, session);
        if(null!=RoomHandler.websocketsession) {
          RoomHandler.sendMessage("refresh");
        }///////////////////////////////to be continued!!!!!
        // System.out.println(user);
        //user.roomNotExist();
        break;
      case "receiveVideoFrom":
        final String senderName = jsonMessage.get("sender").getAsString();
        final UserSession sender = registry.getByName(senderName);
        final String sdpOffer = jsonMessage.get("sdpOffer").getAsString();
        user.receiveVideoFrom(sender, sdpOffer);
        break;
      case "leaveRoom":
        leaveRoom(user);
        if(null!=RoomHandler.websocketsession) {
          RoomHandler.sendMessage("refresh");
        }
        break;
      case "onIceCandidate":
        JsonObject candidate = jsonMessage.get("candidate").getAsJsonObject();

        if (user != null) {
          IceCandidate cand = new IceCandidate(candidate.get("candidate").getAsString(),
              candidate.get("sdpMid").getAsString(), candidate.get("sdpMLineIndex").getAsInt());
          user.addCandidate(cand, jsonMessage.get("name").getAsString());
        }
        break;
      default:
        break;
    }
  }

  @Override
  public void afterConnectionClosed(WebSocketSession session, CloseStatus status) throws Exception {
    UserSession user = registry.removeBySession(session);
    if(null==user){
      return;
    }
    if(null==roomManager.getRoom(user.getRoomName())){
      return;
    }
    roomManager.getRoom(user.getRoomName()).leave(user);
    return;
  }

  private void joinRoom(JsonObject params, WebSocketSession session) throws IOException {
    final String roomName = params.get("room").getAsString();
    final String name = params.get("name").getAsString();
    log.info("pingcoool PARTICIPANT {}: trying to join room {}", name, roomName);

    Room room = roomManager.getRoom(roomName);//没有该room就创建，有就返回该room
    if(room!=null){
      final UserSession user = room.join(name, session);//返回UserSession
      registry.register(user);
      return;
    }
    log.info("room{} is not exist!",roomName);
    JsonObject scParams1 = new JsonObject();
    scParams1.addProperty("id", "roomNotExist");
   synchronized (session) {
      session.sendMessage(new TextMessage(scParams1.toString()));
    }
    return;
  }

  private void leaveRoom(UserSession user) throws IOException {
    final Room room = roomManager.getRoom(user.getRoomName());
    room.leave(user);
   /* if (room.getParticipants().isEmpty()) {
      roomManager.removeRoom(room);
    }*/
  }
}
