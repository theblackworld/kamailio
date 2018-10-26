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

import org.kurento.client.KurentoClient;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.context.annotation.Bean;
import org.springframework.web.socket.config.annotation.EnableWebSocket;
import org.springframework.web.socket.config.annotation.WebSocketConfigurer;
import org.springframework.web.socket.config.annotation.WebSocketHandlerRegistry;

/**
 *
 * @author Ivan Gracia (izanmail@gmail.com)
 * @since 4.3.1
 */
@SpringBootApplication
@EnableWebSocket
public class GroupCallApp implements WebSocketConfigurer {
    //该类实现了 WebSocketConfigurer，就需要实现registerWebSocketHandlers方法，
    // 而该方法在/groupcall这个地址添加一个websocket处理类，
    // 就使得该服务具有websocket的处理能力
    //但是需要告诉springboot调用这个方法来生成一个websocket处理类，
    // 因此在该类名上加上@EnableWebSocket，
    // springboot就知道这个类里一定有一个registerWebSocketHandlers方法，
    // 调用就可以生成一个websocket处理类

    @Override
    public void registerWebSocketHandlers(WebSocketHandlerRegistry registry) {
        System.out.println("registry.addHandler");
        registry.addHandler(groupCallHandler(), "/groupcall");
        registry.addHandler(groupRoomHandler(), "/roomManagement");
     //   registry.addHandler(groupRoomsHandler(), "/roomsManagement");
        //在这里主动调用了一次groupCallHandler()，扫描@Bean之后就不再次调用了
    }

    @Bean//@Bean的意思是，我要获取这个bean的时候，通过运行下面这个方法来获取
    public CallHandler groupCallHandler() {//处理页面websocket的对象
        System.out.println("【5.new CallHandler】");
        return new CallHandler();
    }

    @Bean//@Bean的意思是，我要获取这个bean的时候，通过运行下面这个方法来获取
    public RoomHandler groupRoomHandler() {//处理页面websocket的对象
        System.out.println("【5.new RoomHandler】");
        return new RoomHandler();
    }

    @Bean//@Bean的意思是，我要获取这个bean的时候，通过运行下面这个方法来获取
    public RoomsHandler groupRoomsHandler() {//处理页面websocket的对象
        System.out.println("【5.new RoomsHandler】");
        return new RoomsHandler();
    }

    public GroupCallApp() {
        System.out.println("【2.GroupCallApp】");
    }

    //当springboot遇到了这个方法，就会运行这个方法，这就生成了一个对象
  @Bean
  public UserRegistry registry() {//用户注册集合
      System.out.println("【8.new UserRegistry】");
      return new UserRegistry();
  }

  @Bean
  public RoomManager roomManager() {//房间管理集合
      System.out.println("【6.new RoomManager】");
      return new RoomManager();
  }

  @Bean
  public KurentoClient kurentoClient() {//与kms交互的对象
      System.out.println("【7.KurentoClient.create()】");
      return KurentoClient.create();
  }

  public static void main(String[] args) throws Exception {
      System.out.println("【1.main】");
    SpringApplication.run(GroupCallApp.class, args);
  }


}
