| Run | Codec / resolution / decoder | Private MB | Working set MB | CPU % (all cores) | Cores used |
|---|---|---:|---:|---:|---:|
| app_h264_hw | H.264 720p, hwdec=d3d11va, drops=0 | 211 | 239 | 1.0 | 0.12 |
| app_h264_sw | H.264 720p, hwdec=no, drops=0 | 274 | 310 | 1.4 | 0.16 |
| app_any_hw | H.264 720p, hwdec=d3d11va, drops=0 | 212 | 240 | 1.2 | 0.14 |
| app_any_sw | H.264 720p, hwdec=no, drops=0 | 249 | 279 | 1.2 | 0.15 |
| chrome_hw | avc1.4d401f (136) / opus (251) 720p, decoder=D3D11VideoDecoder, drops=0/1692 | 770 | 1210 | 2.2 | 0.26 |
| chrome_sw | avc1.4d401f (136) / opus (251) 720p, decoder=FFmpegVideoDecoder, drops=0/1789 | 804 | 1242 | 3.0 | 0.37 |
