# Project-ESPNow
未命名，可称为无线小纸条，无线同步触摸或者其他任意二创/开发名称
此为开源项目，适用于常见的ESP32-CYD总成主板。Kurio Reiko感激一切对于本项目的支持，包括语言鼓励、项目转载、代码修缮或者产品再分发以及互补产品研发。商业化销售是被允许的，但是如果有人在未经声明原创作者而直接照搬代码进行商业化销售，那确实挺弱智的。

部分用户可能用的是这套板子：
http://www.lcdwiki.com/zh/2.8inch_ESP32-32E_Display


![f32e759b6bf46be3565b28ebbf2d13e](https://github.com/user-attachments/assets/9870ed31-667e-4ff3-ab37-1c473c22b1a5)


现在提供简明烧录指北，需要的材料有：电脑、板子、数据线、手和眼睛（以Windows为例，我相信会用Linux和MAC的哥们不需要我这样的指北）
## 本文中提到的所有工具在tools分支

## 万事开头
不难，如图安装CH340驱动。无论哪种方式这都是必要的    
前往本仓库下载或使用国内镜像 [国内镜像云盘 密码:fb8b](https://charley-x.lanzoue.com/iMFNB2ixzk1a)


![d4d80c0dc81a3c4baa0582a49186b62](https://github.com/user-attachments/assets/46b36394-1398-42ad-bf2b-eab2394cf620)

以下提供两种烧录方法：ino文件烧录和bin文件烧录。bin方式最简单。

## ino文件烧录
这是原教旨主义的编译和烧录，对ino文件有效，你可分析其代码结构逻辑，以及二创和调试

### 1.下载软件和库
去[官网](https://www.arduino.cc/en/software)把Arduino IDE下载好，选择你的版本

![8530af4605c7a8154c7da3b1ee96273](https://github.com/user-attachments/assets/0cfb522a-4bcd-4fe0-ae9b-543b141c9642)

软件安装好后请安装库，开发板管理器中安装ESP32库、其他库中安装TFT_eSPl和XPT2046_Touchscreen还有SD如下图所示

![0de05df178e2b08b54926ae1c88cdae](https://github.com/user-attachments/assets/90418fd9-bcc1-4446-a9f6-36c669638444)
![440da2c0a98be0fd6a5adc249efe432](https://github.com/user-attachments/assets/6398bf2c-6574-4ca7-817d-8f34544c15ce)
![6537afb5af77f23defc3eefc73c458d](https://github.com/user-attachments/assets/acfadf4d-358b-4e83-a0da-104a2d96e4e2)
<img width="364" height="243" alt="1 }X`W72BQTG4IR_`QSO})V" src="https://github.com/user-attachments/assets/d3bff793-fb25-41a7-8de2-f93e9af46afd" />


检查一下，TFT_eSPl和XPT2046_Touchscreen还有SD同样重要，不要漏了

#### 请注意，ESP32库耗时漫长且不一定能一次下载成功，可能需要梯子。如果不行，可以参考https://blog.csdn.net/Lucife0/article/details/145548563进行离线安装

### 2.修改TFT的setup.h
<del>（详见主页User_Setup.h和其相关介绍）</del> 已经不需要了。

### 3.配置端口和版型

现在，右键你的”此电脑“，打开”管理“，找到”设备管理器“。现在把你的板子连上电脑并在”端口“处找到你的设备

![0c4a891045e8e631deafaea6ec9a83d](https://github.com/user-attachments/assets/3b208fe4-e07d-4867-b74a-26949bc55ed0)
以我为例，我的板子是COM26端口（请不要用分线器/扩展坞）

然后回到你的Arduino IDE软件界面，顶部 工具-开发板=esp32 中找到”ESP32 Dev Module“或者”ESP32 WROOM DA Module“选中，
然后在这里选择你的端口

![54eb715eb5ece521ecd3436cc8143f6](https://github.com/user-attachments/assets/5e0fd326-44cc-4df1-908c-28d41dca3d0f)

到这里就结束了。一切良好的情况下，倘若你的库安装完全，和我的板子引脚也完全相同，那么点击中间的箭头，然后静候烧录完成即可。


## bin文件烧录
俗话说，Facker VS Bin，这和我们现在干的事情一点关系都没有。bin文件烧录适合在懒得研究Arduino IDE的情况下直接快速烧录出成品。但是bin文件不具备学习和代码调试相关的一切功能。但是他快，适合量产或者小白更新软件。

### 1.下载软件
现在马上立刻，去把首页flash_download_tool_3.9.3.zip下载下来然后解压，打开flash_download_tool_3.9.3.exe   
前往本仓库下载或使用国内镜像 [国内镜像云盘](https://charley-x.lanzoue.com/iS4C82ixzllg)    


### 2.选择版本
如图（图片供参考，实际你要选择的型号其实是ESP32）

![9977090efb2aa20c84130d79df0dfe7](https://github.com/user-attachments/assets/189b8aaa-ca71-48c6-a1c4-934842593171)

### 3.烧录

对于已编译的二进制bin文件，你通常会得到一堆bin文件，你只需要xxx.ino.bin 以及 xxx.ino.bootloader.bin 以及 xxx.ino.partitions.bin
其顺序和烧录地址如下：

![6bbc1be7cedc9fd12f9a844735493b5](https://github.com/user-attachments/assets/88259832-018c-4c3b-8c12-e20f420bb7ed)

右下角的COM为你的端口，请自行选择你的端口。

总之记住：bootloader.bin在前，partitions.bin在后，ino.bin最后。

ps：如果可以，你可以尝试把SPI SPEED跳到80MHz，把SPI MODE调成QIO。如果烧录不进去或者出毛病就改回来（或者调整BAUD值为921600）。如果你的bin文件只有一个，那么它的地址是0.



## 总之非常感谢你能读到这里，如果有兴趣，可以移步至我的个人b站账号 https://space.bilibili.com/341918869 以及相关吹水群 391437320

## 鸣谢
在本项目这个草台班子的发展中进行过小批量的成品发售，在这里感谢所有对于第0批、第1批有线供电版和第2批电池供电版本激情下单的朋友们。虽然在发货时效和物流运输方面出现了一些问题，但最后终于是解决了。项目净利润几百块，但是晚上在近邻宝发货捡泡沫纸和纸壳的时候把apple pencil搞丢了，导致最终早期收益大致为负，lol。无所谓了，总之感谢各位的鼎力支持，这一点才是最重要的。致谢名单如下（排名不分先后）：

岚夕Lux  百川之鱼  晓嘿君233  乡村充电宝  躺平青年  Wowaka39  鱼鱼心系天下  懒趴趴  865770394  亦晨x  WANGZHENLE  宇宙尘埃  最怕多情负美人  风  小万同学  彼岸落雁er  whyt  赤耳大仙  我有异议症  鹰鹰鹰_  桃儿卡卡  拖宝狂爱粉   Haruka1z  雾师傅千夜  Irdescent  乐高乐高乐  莱博维茨  五好少女  路过的魔法师  咸鱼  来咯个梨  三岁的随便  55181860  二环立交桥下等风雨  机械银狼  扩展爱好的筱瑶  田筒不是田筒  名字学学我别太长  有你的快递到了  AlanWalkernzy  QWQ  Miner  辛连  路过的魔法师  六柒玖肆 小鱼鱼鱼正 乐呵呵918 ashaf 未来的极客 易青乔H 年糕炸鱼薯条   可***玲 一眼看上机 𐂂 重庆纸上飞包装 UF6_Pro 茕月佐诗 啊啊车车

其中，群友 机械银狼 同时负责了外壳的建模和制作，地址https://makerworld.com.cn/zh/models/771017#profileId-739370

1.0.5版本的更新是在群友 QWQ 的督促下完成的

群友xiao_hj909贡献了1.0.6版本更新。

1.1.0爱来自砂纸鸽（Shapaper223）

向以上所有人表示感谢！



