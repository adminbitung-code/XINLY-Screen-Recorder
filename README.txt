XINLY SCREEN RECORDER V2

Fitur:
- Screen capture 720p/1080p 30 FPS
- Audio OFF / Internal Audio / Microphone / Internal + Mic
- Bitrate 2 / 4 / 6 Mbps
- H.264 hardware encoder h264_qsv
- MP4
- F9 Start/Stop
- F10 Pause/Resume (UI/control layer)
- Output Videos\XINLY Recordings
- Dirancang untuk beban rendah

CATATAN AUDIO:
Internal Audio memakai WASAPI loopback device "default".
Microphone pada mode ini memakai DirectShow "default"; pada sebagian PC,
nama device microphone harus disesuaikan di command FFmpeg jika "default"
tidak ditemukan.

CATATAN:
Versi ini memprioritaskan 720p/30/2 Mbps untuk Intel N4020.
Jika h264_qsv tidak tersedia, ganti encoder dengan libx264 -preset ultrafast -crf 28.

BUILD:
Push source ini ke GitHub. Workflow Windows akan menghasilkan ZIP berisi
EXE + ffmpeg.exe.
