# stream.ps1 - FFmpeg RTP to WSL; SDP at /tmp/yuyv.sdp (WinPS 5.1 compatible)
$ErrorActionPreference = 'Stop'

#function Hold($m='Press Enter to exit...'){ try{ Read-Host $m | Out-Null } catch{ Start-Sleep -Seconds 3600 } }

# ===== Config =====
$Distro    = 'Ubuntu-22.04'
$Device    ='USB2.0 PC CAMERA'
# $Device    = 'USB Camera' #相机名称
# $Device    = 'Nantian Camera 8713'
$VideoSize = '640x480'
$Framerate = 30
$PixIn     = 'yuyv422'   # input
$PixOut    = 'uyvy422'   # output
$Port      = 5004
$Payload   = 96
$PktSize   = 1200
# ===================
# 获取WSL IP
$WSL_IP = $null
$ipLine = (wsl -d $Distro hostname -I) -as [string]
if ($ipLine) {
  $WSL_IP = ($ipLine -split '\s+') | Where-Object { $_ -match '^\d{1,3}(\.\d{1,3}){3}$' } | Select-Object -First 1
}

ffmpeg -f dshow -video_size 640x480 -framerate 30 -pixel_format yuyv422 `
  -rtbufsize 4M -use_video_device_timestamps 1 -thread_queue_size 4 `
  -i video=$Device `
  -fps_mode passthrough -vsync passthrough `
  -fflags nobuffer -flags low_delay -max_delay 0 -muxpreload 0 -muxdelay 0 `
  -c:v rawvideo -pix_fmt uyvy422 `
  -f rtp -payload_type 96 `
  -sdp_file \\wsl.localhost\$Distro\tmp\yuyv.sdp `
  "rtp://$($WSL_IP):5004?pkt_size=1200"



#2.windows 打开powershell 启动推流
#ffmpeg -f dshow -video_size 640x480 -framerate 30 -pixel_format yuyv422 `
#  -rtbufsize 4M -use_video_device_timestamps 1 -thread_queue_size 4 `
#  -i video="USB Camera" `
#  -fps_mode passthrough -vsync passthrough `
#  -fflags nobuffer -flags low_delay -max_delay 0 -muxpreload 0 -muxdelay 0 `
#  -c:v rawvideo -pix_fmt uyvy422 `
#  -f rtp -payload_type 96 `
#  -sdp_file \\wsl.localhost\Ubuntu-22.04\tmp\yuyv.sdp `
#  "rtp://172.23.117.167:5004?pkt_size=1200"