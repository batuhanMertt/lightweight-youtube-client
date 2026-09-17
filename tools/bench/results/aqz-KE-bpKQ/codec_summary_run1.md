Video: https://www.youtube.com/watch?v=aqz-KE-bpKQ
First run, 17 September 2026 16:50. (codec_summary.md is the second run, 17:07. The per-run
.json/.txt files in this folder belong to the second run.)

| Run | What was actually decoded | Private MB | Working set MB | CPU % (all cores) | CPU cores used | Dropped frames | Video advanced during 30 s |
|---|---|---:|---:|---:|---:|---:|---:|
| App, H.264, GPU decoding (default) | h264 1280x720@60, GPU (d3d11va) | 217 | 241 | 1.9 | 0.22 | 11 | 34.2 s |
| App, H.264, CPU decoding | h264 1280x720@60, CPU | 256 | 285 | 2.6 | 0.31 | 0 | 33.1 s |
| App, VP9 forced, CPU decoding | vp9 1280x720@60, CPU | 231 | 260 | 2.9 | 0.34 | 1 | 33.1 s |
| App, AV1 forced, CPU decoding | av1 1280x720@60, CPU | 243 | 272 | 4.5 | 0.54 | 0 | 33 s |
| Chrome, GPU decoding (default) | av01.0.08M.08 (398) / opus (251) 1280x720@60, GPU (D3D11VideoDecoder) | 861 | 1283 | 2.8 | 0.33 | 0 of 4316 | 32.7 s |
| Chrome, CPU decoding (--disable-accelerated-video-decode) | av01.0.08M.08 (398) / opus (251) 1280x720@60, CPU (Dav1dVideoDecoder) | 910 | 1343 | 7.7 | 0.93 | 1 of 4297 | 32.3 s |
