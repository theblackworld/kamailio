package org.kurento.tutorial.groupcall;


import org.kurento.client.KurentoClient;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Controller;
import org.springframework.ui.Model;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestMethod;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.ResponseBody;

import java.io.IOException;
import java.util.*;


//通过反射对该对象进行实例化，springboot通过扫描类目录下包含@Controller注释的类，通过反射实例化这个类
@Controller
public class HelloController {
    @Autowired
    //告诉springboot，该类依赖下面的类，springboot需要将依赖注入到这里
    private RoomManager roomManager;

    @Autowired
    private KurentoClient kurento;

    public HelloController() {
        System.out.println("【3.HelloController】");
    }


    //测试模版引擎
    @RequestMapping(value = "/success", method = RequestMethod.GET)
    public String success2() {
        System.out.print("thymeleaf in use!");

        return "success";
    }
    //测试roomManager跨类使用
    @RequestMapping(value = "/roomManagertest", method = RequestMethod.GET)
    public String roomManager() {
        System.out.print(roomManager.rooms.containsKey("1"));

        return "success";
    }
    //测试页面查询功能
    @RequestMapping(value = "/management", method = RequestMethod.GET)
    public String inquire(Model model) {
        model.addAttribute("rooms",roomManager.rooms);
        return "management";
    }

    //add
    @RequestMapping(value = "/add", method = RequestMethod.POST)
    public String add(@RequestParam("roomID") String roomName, @RequestParam("Presenter") String presenter,@RequestParam("Scale") String scale,Model model) {
        Room room = new Room(roomName, kurento.createMediaPipeline(), presenter, Integer.valueOf(scale).intValue());
        roomManager.rooms.put(roomName, room);
        model.addAttribute("rooms",roomManager.rooms);
        return "management";
    }
    //delete
    @RequestMapping(value = "/delete", method = RequestMethod.POST)
    public String delete(@RequestParam("roomID") String roomName, @RequestParam("Presenter") String presenter,@RequestParam("Scale") String scale,Model model) {
        System.out.println("try to delete room:【"+roomName+"】 Presenter:【"+presenter+"】");
        Room r1=roomManager.rooms.get(roomName);
        System.out.println(r1);

        if(null!=r1){

            Collection<UserSession> c1= r1.getParticipants();
            if(c1.isEmpty()){
                //说明room刚刚创建，并没有与会者进入过,此时由管理者删除空房间
                roomManager.rooms.remove(roomName);
            }
            Iterator iterator = c1.iterator();
            while(iterator.hasNext()){
                UserSession us=(UserSession)iterator.next();
                System.out.println("closing "+us.getName());
                try {
                    us.closedDyServer();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
        System.out.println("111");

        while(null!=roomManager.rooms.get(roomName)){
            if(roomManager.rooms.get(roomName).getParticipants().isEmpty()){
                //说明room刚刚创建，并没有与会者进入过,此时由管理者删除空房间
                roomManager.rooms.remove(roomName);
            }
        }
        System.out.println("222");

        model.addAttribute("rooms",roomManager.rooms);
        return "management";
    }

    //房间详细信息
    @RequestMapping(value = "/participants", method = RequestMethod.POST)
    public String participant(@RequestParam("roomID") String roomName, @RequestParam("Presenter") String presenter,@RequestParam("Scale") String scale,Model model) {

        model.addAttribute("room",roomManager.rooms.get(roomName));
        return "room";
    }

    //移除与会者
    @RequestMapping(value = "/remove", method = RequestMethod.POST)
    public String remove(@RequestParam("roomName") String roomName, @RequestParam("participant") String participant,Model model) {
        try {
            roomManager.rooms.get(roomName).getParticipant(participant).closedDyServer();
        } catch (IOException e) {
            e.printStackTrace();
        }
        while(null!=roomManager.rooms.get(roomName).getParticipant(participant));
        model.addAttribute("room",roomManager.rooms.get(roomName));
        return "room";

    }
}