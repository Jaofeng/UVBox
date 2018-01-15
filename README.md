# 自製 UV 紫外線曝光控制器
本控制器以 Arduino 的 ATMEGA328P 為核心，控制 UV LED 燈板，作為 DIY PCB 曝光用。

## 目錄
* [控制器硬體概要說明](#控制器硬體概要說明)
* [Arduino 程式功能](#arduino程式功能)
* [原理圖與板線圖](#原理圖與板線圖)
* [實做範例](#實做範例)


## 控制器硬體概要說明
控制器分為兩版本：595 版與 I2C LCD 版。  
兩版本電路、功能都大同小異，最大差別在於 595 版是使用 74x595 控制七段顯示器作為顯示；I2C LCD 則是使用 PCF8574 控制 1602 的 LCD(相容HD44780)。  
I2C LCD 版提供 USB 界面(使用 FT232R)，可直接修改 Arduino 程式；而 595 版本就必須使用 ICSP 界面進行更新。

使用 4 + 1 個按鍵共 5 個按鍵進行控制：
* Start : 開始曝光
* Mode : 選擇模式  
    * 同時點亮上下兩板  
    * 僅點亮上板  
    * 僅點亮下板  
    * 亮度調整模式  
* Up / Dn :  
    * 亮度調整模式：增加/減少亮度值，增減量 +/- 5%
    * 其餘模式：增加/減少秒數，增減量 10 秒
* Reset : 重新啟動

595 版是第一個版本，小弟先用麵包版電路作第一片 595 版的 PCB，再用他去做其他的 PCB。

使用一般 Notebook 電源供應器(DC 19V)做為電源輸入，由於電路主要電壓使用 5V，為避免壓差太大，造成降壓晶片過熱燒毀，所以我做了兩次降壓：19v -> 12v -> 5v。  
由 ULN2003 控制 UV LED 燈板，也就是燈板部分直接使用 19V 電源。

在此不提供 UV LED 燈板的電路，我使用串 6 並 7 的方式組成一片燈板，意思就是 6 顆 UV LED 與 1 顆限流電阻為一組做串連，一共有 7 組。  
LED 間距約 7 x 2.54 mm。

依 LED 編排密度、曝光亮度、燈板高度、線路複雜度、感光 PCB 有效期、遮色片材質等等因素，都會互相影響，所以建議各位自行實驗，找出適合的亮度、時間與燈板高度。

以小弟的經驗是：
* 線路片使用噴墨投影片，兩張重疊，增加黑色遮光效果
* 燈板高度約 7cm
* 亮度 70%
* 時間 90s~110s 不等

以上述條件，即可成功製作一片漂亮的 PCB 線路。

對了，很基本的一件事：遮色片與感光 PCB 一定要夾緊，不能有任何間隙，否則非常容易失敗。

***

## Arduino 程式功能
開始曝光後，會顯示倒數時間，曝光時間結束時會將燈板關閉且蜂鳴器會發出警示音。

控制功能：
* 可單一控制上下燈板
* 亮度調整範圍自 0% ~ 100%
* 時間可調整範圍自 0s ~ 600s
* 曝光時間結束時，自動關閉光板，並響起警示音
* 警示音響起時，按任何一鍵停止響音

***
## 原理圖與板線圖
595版  
<img src="/Images/595_SCH.png" alt="595 版原理圖" title="595 版原理圖" width="200" />
<img src="/Images/595_BRD_F.png" alt="595 正面板線圖" title="595 正面板線圖" width="200" />
<img src="/Images/595_BRD_B.png" alt="595 背面板線圖" title="595 背面板線圖" width="200" />

I2C LCD版  
<img src="/Images/I2C_LCD_SCH.png" alt="I2C LCD 版原理圖" title="I2C LCD 版原理圖" width="200" />
<img src="/Images/I2C_LCD_BRD_F.png" alt="I2C LCD 正面板線圖" title="I2C LCD 正面板線圖" width="200" />
<img src="/Images/I2C_LCD_BRD_B.png" alt="I2C LCD 背面板線圖" title="I2C LCD 背面板線圖" width="200" />

***
## 實做範例
UV LED 燈板與控制板(595)  
<img src="/Images/Demo3.jpg" title="燈板與控制板(未亮)" width="250" /><img src="/Images/Demo4.jpg" title="燈板與控制板(點亮)" width="250" /><img src="/Images/Demo7.jpg" title="底片與壓克力夾板" width="250" /><img src="/Images/Demo8.jpg" title="燈板與壓克力夾板放置位置" width="250" /><img src="/Images/Demo5.jpg" title="曝光中-側面" width="250" /><img src="/Images/Demo6.jpg" title="曝光中-俯視" width="250" />

595 版正反面  
<img src="/Images/Demo1.jpg" title="595版正反面" height="200" /><img src="/Images/Demo2.jpg" title="595版正面" height="200" />

自製 USB to UART 轉接板  
<img src="/Images/USB2UART-F.jpg" title="USB to UART 正面" height="60" /><img src="/Images/USB2UART-B.jpg" title="USB to UART 背面" height="60" />
