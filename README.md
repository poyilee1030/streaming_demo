<p float="left">
  <img src="/snapshot.jpg" width="480" />
</p>

這個項目主要是展示了使用 QT + FFmpeg 讀取 input
並將畫面以 OpenGL 呈現出來
同時提供 photo capture & video recording 功能

<p float="left">
  <img src="/usb_video_capture.jpg" width="480" />
</p>

原本專案是使用一個 usb video capture
但考量大多數人並不會有這個裝置
所以我稍微修改了一下
讓程式可以用 usb camera 作為輸入使用
作為 demo 使用，主要是想讓程式能夠執行起來
如果有 crash 或 bug 請多包涵

使用方法
1. 下載 demo_release.7z
2. 解壓縮後執行 StreamingDemo.exe
3. 執行後會在目錄下產生 list_devices.txt
4. 以 list_devices_example1.txt & list_devices_example2.txt 為例
   剛好兩個檔案都是 i=1 是 video stream input 而 i=4 是 audio steam input
6. 自行在目錄下建立一個 device_idx.txt，裡面填入 「1,4」
7. 理論上完成步驟 5 後即可運行(親測2台 usb camera可運行)

-------------------------------------------------------------------------------------------------

This project primarily demonstrates the use of QT + FFmpeg to read input and render the video using OpenGL. 
It also provides photo capture and video recording functions.

The original project used a USB video capture device, but considering that most people don't have access to this device. 
I made a small modification to allow the program to use a USB camera as input. 
The main purpose of the demo is to get the program running. 
Please bear with any crashes or bugs.

Instructions:
1. Download demo_release.7z
2. Extract and run StreamingDemo.exe
3. After running, a list_devices.txt file will be generated in the directory
4. Using list_devices_example1.txt and list_devices_example2.txt as examples,
   in both files i=1 is the video stream input, and i=4 is the audio stream input
6. Create a device_idx.txt file in the directory and enter "1,4" into the file
7. In theory, after completing step 5, the program should run (tested with 2 USB cameras and confirmed to work)
