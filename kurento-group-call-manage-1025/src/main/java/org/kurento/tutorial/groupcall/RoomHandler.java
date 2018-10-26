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
public class RoomHandler extends TextWebSocketHandler {
    public static WebSocketSession websocketsession;

    @Override
    public void handleTextMessage(WebSocketSession session, TextMessage message)
            throws Exception {
        this.websocketsession = session;
        System.out.println("【ManagementHandler】message :" + message);
    }

    public static synchronized void sendMessage(String message) {
        try {
            websocketsession.sendMessage(new TextMessage(message));
        } catch (IOException ex) {
        }
    }

    @Override
    public void afterConnectionClosed(final WebSocketSession session,
                                      CloseStatus status) throws Exception {
        // stop(session);
        this.websocketsession = null;
    }
}

