Video: https://www.youtube.com/watch?v=aqz-KE-bpKQ

| Run | What was actually decoded | Private MB | Working set MB | CPU % (all cores) | CPU cores used | Dropped frames | Video advanced during 30 s |
|---|---|---:|---:|---:|---:|---:|---:|
| App, H.264, GPU decoding (default) | h264 1280x720@60, GPU (d3d11va) | 217 | 241 | 2.0 | 0.24 | 11 | 34.2 s |
| App, H.264, CPU decoding | h264 1280x720@60, CPU | 256 | 285 | 3.1 | 0.38 | 2 | 34.1 s |
| App, VP9 forced, CPU decoding | vp9 1280x720@60, CPU | 230 | 259 | 3.1 | 0.38 | 0 | 33 s |
| App, AV1 forced, CPU decoding | av1 1280x720@60, CPU | 243 | 271 | 4.3 | 0.51 | 0 | 33.1 s |
| Chrome, GPU decoding (default) | av01.0.08M.08 (398) / opus (251) 1280x720@60, GPU (D3D11VideoDecoder) | 846 | 1263 | 2.7 | 0.32 | 0 of 4309 | 32.6 s |
| Chrome, CPU decoding (--disable-accelerated-video-decode) | av01.0.08M.08 (398) / opus (251) 1280x720@60, CPU (Dav1dVideoDecoder) | 939 | 1357 | 7.8 | 0.93 | 1 of 4315 | 32.6 s |
