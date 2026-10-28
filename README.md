# Astro-Sentinel VLM Droid — VENTUNO Q + Arduino App Lab 0.11+

Astro-Sentinel is a privacy-first, R2-D2-inspired physical companion. The Arduino USB Camera captures locally, the App Lab **Vision-Language Model (VLM) Brick** generates a short text label on-device, NVIDIA Nemotron reasons about that label via Nebius Token Factory, and the VENTUNO Q reacts with onboard LED-matrix animations, Modulino Buzzer chirps, and Vibro pulses.

## Data flow

```text
Arduino USB Camera (USB UVC; local frame capture)
    │  local, in-memory frame buffer
    ▼
Arduino App Lab VLM Brick (VisionLanguageModel / local Qwen3-VL model)
    │  short text label, e.g. "blue ceramic mug"
    ▼
Nebius Token Factory / NVIDIA Nemotron 3.5 Lightning
    │  HAPPY | CURIOUS | CONFUSED
    ▼
VENTUNO Q microcontroller via Bridge
    ├── onboard 8×13 LED matrix: camera-active marker + droid animations
    ├── Modulino Buzzer: R2-D2-style chirps
    └── Modulino Vibro: haptic acknowledgement

Modulino Distance ── triggers an interaction at 300 mm; exits at 360 mm
```

## Important behavior and privacy

- The camera stream is captured continuously on the board. The matrix shows a **persistent blinking camera glyph** while fresh frames are arriving. A crossed-camera glyph appears when frames stop or the camera is unavailable.
- The VLM does not run on every frame. When an object enters the distance zone, a local snapshot is labelled; while the object remains in-zone, the VLM re-labels every `VLM_REPEAT_INTERVAL_SECONDS` after the previous analysis completes. This is continuous interaction without flooding the VLM or Nebius.
- The local VLM Brick's documented API takes image paths. For each label request, the app compresses one fresh frame and writes a short-lived JPEG under `python/.astro_runtime` so the local Brick can access its path. The JPEG is deleted immediately when local VLM inference returns, before the Nebius request. No image is sent to Nebius or retained as a project asset. The image exists briefly on local storage, so this is **not** a zero-disk-image implementation.
- Only sanitized text label plus distance are sent to Nebius. The VLM prompt asks the local model to ignore people; `astro_logic.py` also drops labels that look person-centred. This is a best-effort guard, not a guarantee of anonymization.
- If the cloud key/request fails, local sensing continues and the droid gives a gentle `CONFUSED` fallback.
- The board matrix, rather than the camera ring light, is used as the camera-active indicator. The camera's integrated ring light is illumination, not a reliable in-use/privacy indicator.

## Hardware

- Arduino VENTUNO Q, powered from its recommended supply
- Arduino USB Camera plugged into a USB-A port on VENTUNO Q
- Modulino Distance, Modulino Buzzer, Modulino Vibro
- Network access for the Nemotron stage (local VLM itself is configured through App Lab)

The Arduino USB Camera is a UVC device and is documented for direct use on VENTUNO Q without a separate driver. The camera capture source is configured as `usb:0` by default.

## Files

```text
Astro-Sentinel-VLM-Physical-AI/
├── app.yaml                       # declares arduino:vlm
├── python/
│   ├── main.py                    # Camera capture + local VLM + Nemotron pipeline
│   ├── astro_logic.py             # label sanitization and mood parsing helpers
│   ├── requirements.txt           # OpenAI-compatible Nebius client
│   └── .env.example               # Token Factory configuration template
├── sketch/
│   ├── sketch.ino                 # matrix camera indicator, state animations, Modulino RPC
│   └── sketch.yaml                # VENTUNO Q + Modulino libraries
├── tests/test_astro_logic.py
└── docs/HACKATHON_DEMO.md
```

## Step-by-step installation

### 1. Back up the existing project

Stop the working Astro-Sentinel app and make a copy of its folder. You do **not** need to reinstall App Lab or change your working VENTUNO Q board setup.

### 2. Import/copy this project

Import the ZIP through App Lab's normal app-import flow, or copy these files into a new app preserving the folder layout. Keep your board target set to `arduino:zephyr:ventunoq` and leave the installed Zephyr platform version unpinned.

### 3. Configure the VLM Brick

Open the app's **Bricks** panel and add/confirm **VLM** (`arduino:vlm`). In the Brick's model configuration, download/select the local **Qwen 3-VL-8B VLM** model available in App Lab 0.11.0. The model must be installed locally before the first inference. This project uses the official Python API shape:

```python
from arduino.app_bricks.vlm import VisionLanguageModel
vlm = VisionLanguageModel(system_prompt="...")
label = vlm.chat("Return a concise object label only.", images=["/path/to/local-snapshot.jpg"])
```

The app does **not** use the Video Object Detection Brick in this edition; the VLM generates natural-language labels directly from snapshots.

### 4. Configure Nebius Token Factory

Create a Token Factory API key and configure `python/.env` (copy `.env.example` and edit it). Keep the real `.env` private:

```dotenv
NEBIUS_API_KEY=your_actual_token_factory_api_key
NEBIUS_BASE_URL=https://api.tokenfactory.nebius.com/v1/
NEBIUS_MODEL=nvidia/Nemotron-3_5-Lightning

CAMERA_SOURCE=usb:0
CAMERA_WIDTH=640
CAMERA_HEIGHT=480
CAMERA_FPS=8
CAMERA_ACTIVE_TIMEOUT_SECONDS=4.5
TRIGGER_DISTANCE_MM=300
EXIT_DISTANCE_MM=360
DISTANCE_POLL_INTERVAL_SECONDS=0.12
VLM_REPEAT_INTERVAL_SECONDS=10
VLM_IMAGE_JPEG_QUALITY=82
VLM_MAX_LABEL_WORDS=8
NEMOTRON_TIMEOUT_SECONDS=18
REACTION_HOLD_SECONDS=3.0
```

Make sure the key is for **Nebius Token Factory**, not a general Nebius Cloud IAM key. Set file permissions to owner-only if you use the board shell (`chmod 600 python/.env`). Never put the real key in Git or screenshots.

### 5. Keep your known-good Modulino version

The sketch declares `Arduino_Modulino (0.10.0)`, which is the library version that fixed your earlier setup issue. It retains the known transitive dependencies from your working build profile. Do not install a new board core just for this application. If App Lab lists library resolution issues, use its Sketch Library panel and match the missing header's library rather than changing the FQBN.

### 6. Connect camera and Qwiic modules

Plug the Arduino USB Camera into one of VENTUNO Q's USB-A ports. Connect Modulino Distance, Buzzer, and Vibro through the same working Qwiic arrangement you already validated. Aim the camera and distance sensor at the same presentation area.

### 7. Run the app

Press **Run** in App Lab. At startup the matrix initially shows the offline/idle symbol until the camera capture loop supplies a fresh frame, then the small camera glyph at the top-right starts pulsing. Typical logs:

```text
Astro-Sentinel VLM Droid starting: VENTUNO Q + Arduino USB Camera + local VLM + Nemotron
[camera] Stream started from usb:0 at 640x480/8 fps.
[privacy] VLM runs locally. A short-lived JPEG in the app-local `.astro_runtime` folder is deleted immediately after local VLM inference; Nebius receives only the sanitized text label and distance.
[vlm] Using Arduino App Lab VisionLanguageModel Brick; select/download Qwen 3-VL-8B VLM in Brick settings.
[hardware] distance=ok,buzzer=ok,vibro=ok,led_matrix=ok,camera_indicator=ok
[camera] ACTIVE — blinking onboard camera indicator enabled.
```

Exact timing/log order depends on how quickly the selected VLM model initializes.

### 8. Try the droid interaction

1. Wait for the camera glyph to pulse. This means recent frames are arriving.
2. Put an ordinary object in the camera's view and move it within **300 mm** of the distance sensor.
3. The LED matrix shows a sweeping scan while the local VLM labels the object.
4. The matrix switches to a spinner while the text label and distance are sent to Nemotron.
5. Nemotron chooses a friendly mood. The matrix changes to a droid expression and the buzzer/vibration produce a matching pattern.
6. Keep an object in range: another VLM label is attempted every 10 seconds after the previous job completes.
7. Move the object past **360 mm** to return to idle. The system is ready for another presentation immediately; it no longer requires a 450 mm re-arm.

## LED matrix states

The 8×13 blue matrix is animated on the MCU and refreshed continuously. A small camera glyph is overlaid on every state so the recording/stream indicator remains visible while the droid scans or reasons.

| State | Pattern | Meaning |
|---|---|---|
| IDLE | blinking droid eyes/face | Waiting, camera still active if glyph pulses |
| LABELING | sweeping beam with echoes | Local camera snapshot and VLM label generation |
| IDENTIFIED | checkmark frame | A non-unknown local label was generated |
| THINKING | rotating spinner | Nemotron text-only reasoning in progress |
| HAPPY | curved smile + upbeat chirps | Positive acknowledgement |
| CURIOUS | asymmetric eyes/question glyph + rising chirps | Interest/uncertainty |
| CONFUSED | large question mark + descending chirps | Unknown label or unclear input |
| OFFLINE | X pattern | Nebius unavailable/missing key; local sensing continues |
| CAMERA_ERROR | sad/curious droid face + crossed camera icon | No fresh local camera frame or VLM snapshot failed |
| Camera glyph | pulsing dot in a tiny outlined camera | Fresh frames are arriving; crossed glyph means no recent frames |

## Tuning

- `TRIGGER_DISTANCE_MM=300`: start interaction at 30 cm.
- `EXIT_DISTANCE_MM=360`: leave the interaction zone at 36 cm; this is a small hysteresis gap, not the old 450 mm re-arm.
- `VLM_REPEAT_INTERVAL_SECONDS=10`: repeat local labeling while an object is present. Use 12–15 seconds if inference is slow or you want fewer cloud calls.
- `CAMERA_SOURCE=usb:0`: first USB camera. If you have multiple USB cameras, select the correct source listed by the camera examples; don't open another `cv2.VideoCapture()` against the same camera.

## Troubleshooting

- **`No module named arduino.app_bricks.vlm`**: open the Bricks panel, add `arduino:vlm`, and allow App Lab to provision it.
- **VLM starts but reports no model**: download/select Qwen 3-VL-8B VLM in Brick configuration. The first model startup can take longer than subsequent scans.
- **No frame / crossed camera glyph**: reseat the USB camera; test the official Camera basics example; verify that `CAMERA_SOURCE=usb:0` is correct.
- **Matrix doesn't show the camera glyph**: verify the sketch builds against the already-installed VENTUNO Q platform, and check the console for Bridge errors.
- **Nemotron key missing or API fails**: verify `python/.env`, Token Factory key permissions/model access, and the base URL. A fallback mood is used instead of stopping the local application.
- **Compilation again reports `vl53l4cd_class.h` or `SHT31.h`**: use App Lab's Sketch Library panel to add the specifically required driver; preserve `Arduino_Modulino 0.10.0` and the correct FQBN.
- **Labels take too long**: first ensure the local Qwen 3-VL model is downloaded. Reduce capture resolution to 640×480 (the default); increasing the repeat interval reduces workload but does not make one inference faster.

## Official references

- [Arduino USB Camera](https://docs.arduino.cc/hardware/usb-camera)
- [Arduino App Lab 0.11.0 release notes](https://docs.arduino.cc/software/app-lab/release-notes/release-0-11)
- [Official local VLM Brick example](https://github.com/arduino/app-bricks-examples/tree/main/bricks/arduino/vlm/01_simple_chat)
- [Official camera peripheral example](https://github.com/arduino/app-bricks-examples/tree/main/core-and-foundational/06-camera-basics/02-camera-from-sources)
- [Nebius Token Factory NVIDIA Nemotron inference](https://nebius.com/services/token-factory/models/nvidia-nemotron-models-inference)
