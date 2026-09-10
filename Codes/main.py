#!/usr/bin/env python3
import json
import os
import sys
import time
from datetime import datetime
from pathlib import Path

import cv2
import numpy as np
import serial
import serial.tools.list_ports

os.environ["OPENCV_VIDEOIO_PRIORITY_MSMF"] = "0"

BASE_DIR = Path(__file__).resolve().parent

# System Configurations
SERIAL_PORT = "COM3"
BAUD_RATE = 9600
CAMERA_INDEX = 0

GO_TO_CAMERA_SIGNAL = b"C\n"
HOME_SIGNAL = b"H\n"
READY_SIGNAL = "READY"
ACK_SIGNAL = b"DONE\n"

WATCH_DIR = BASE_DIR / "incoming"
TEMP_DIR = BASE_DIR / "capture_tmp"

REFERENCE_FRAME = (640, 480)
SHEET_CELL_ROI = (242, 110, 448, 314)


def find_arduino_port():
    """Locate connected microcontroller COM port automatically."""
    ports = list(serial.tools.list_ports.comports())
    for port in ports:
        description = port.description or ""
        if any(keyword in description for keyword in ["Arduino", "CH340", "USB-SERIAL", "Serial"]):
            return port.device
    if ports:
        return ports[0].device
    return None


def capture_image_opencv():
    """Capture a single frame using DirectShow API to bypass Windows lock."""
    WATCH_DIR.mkdir(parents=True, exist_ok=True)
    TEMP_DIR.mkdir(parents=True, exist_ok=True)

    filename = f"sheet_{datetime.now():%Y%m%d_%H%M%S_%f}.png"
    final_path = WATCH_DIR / filename

    print(f"[+] Initializing camera (Index: {CAMERA_INDEX})...")
    cap = cv2.VideoCapture(CAMERA_INDEX, cv2.CAP_DSHOW)

    if not cap.isOpened():
        cap = cv2.VideoCapture(CAMERA_INDEX)

    if not cap.isOpened():
        raise RuntimeError(f"Unable to access camera index {CAMERA_INDEX}.")

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    # Warm-up frames for auto-exposure adjustment
    for _ in range(10):
        ret, frame = cap.read()
        time.sleep(0.03)

    if not ret or frame is None:
        cap.release()
        raise RuntimeError("Failed to capture frame from target camera.")

    cv2.imwrite(str(final_path), frame)
    cap.release()
    print(f"[+] Frame successfully captured: {final_path.name}")
    return final_path


def auto_detect_black_plate(image):
    """Detect the central black tray area within the captured image."""
    hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
    lower_black = np.array([0, 0, 0])
    upper_black = np.array([180, 255, 75])
    mask = cv2.inRange(hsv, lower_black, upper_black)

    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if contours:
        h, w = image.shape[:2]
        center_x, center_y = w // 2, h // 2

        valid_contours = []
        for c in contours:
            x, y, bw, bh = cv2.boundingRect(c)
            if bw > w * 0.25 and bh > h * 0.25:
                dist = np.hypot(x + bw / 2 - center_x, y + bh / 2 - center_y)
                valid_contours.append((dist, (x, y, x + bw, y + bh)))

        if valid_contours:
            valid_contours.sort(key=lambda item: item[0])
            return valid_contours[0][1]

    return None


def load_and_crop_pills(image_path, output_dir="temp_pills", target_size=128):
    """Segment individual pill grid cells using adaptive Otsu thresholding."""
    os.makedirs(output_dir, exist_ok=True)
    image = cv2.imread(str(image_path))
    if image is None:
        raise FileNotFoundError(f"Image resource not found: {image_path}")

    h, w = image.shape[:2]

    detected_roi = auto_detect_black_plate(image)
    if detected_roi:
        left, top, right, bottom = detected_roi
    else:
        rw, rh = REFERENCE_FRAME
        left = int(SHEET_CELL_ROI[0] * w / rw)
        top = int(SHEET_CELL_ROI[1] * h / rh)
        right = int(SHEET_CELL_ROI[2] * w / rw)
        bottom = int(SHEET_CELL_ROI[3] * h / rh)

    pad_w = int((right - left) * 0.03)
    pad_h = int((bottom - top) * 0.03)
    left += pad_w
    top += pad_h
    right -= pad_w
    bottom -= pad_h

    xs = np.rint(np.linspace(left, right, 6)).astype(int)
    ys = np.rint(np.linspace(top, bottom, 6)).astype(int)

    debug = image.copy()
    pills, metadata, tiles = [], [], []

    for r in range(5):
        for c in range(5):
            x1, x2 = int(xs[c]), int(xs[c + 1])
            y1, y2 = int(ys[r]), int(ys[r + 1])
            cell = image[y1:y2, x1:x2]

            gray = cv2.cvtColor(cell, cv2.COLOR_BGR2GRAY)
            blurred = cv2.GaussianBlur(gray, (3, 3), 0)

            # Dynamic thresholding for detection of damaged/partial fragments
            _, thresh = cv2.threshold(blurred, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
            contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

            # Lower threshold limit (0.8%) to catch tiny broken pill pieces
            cell_area = cell.shape[0] * cell.shape[1]
            min_area = int(cell_area * 0.008)

            valid_contours = [cnt for cnt in contours if cv2.contourArea(cnt) >= min_area]
            found = len(valid_contours) > 0

            if found:
                main_contour = max(valid_contours, key=cv2.contourArea)
                cx, cy, cw, ch = cv2.boundingRect(main_contour)

                center_x, center_y = cx + cw // 2, cy + ch // 2
                side = int(max(cw, ch) * 1.75)

                sx, sy = max(0, center_x - side // 2), max(0, center_y - side // 2)
                tx, ty = min(cell.shape[1], center_x + side // 2), min(cell.shape[0], center_y + side // 2)

                crop = cell[sy:ty, sx:tx].copy()
                box = [x1 + sx, y1 + sy, x1 + tx, y1 + ty]
            else:
                crop = cell.copy()
                box = [x1, y1, x2, y2]
                print(f"[!] Cell R{r + 1}C{c + 1} registered as empty.")

            if target_size and crop.size > 0:
                crop = cv2.resize(crop, (target_size, target_size), interpolation=cv2.INTER_CUBIC)

            path = os.path.join(output_dir, f"pill_r{r + 1}_c{c + 1}.png")
            cv2.imwrite(path, crop)

            pills.append({"row": r + 1, "col": c + 1, "path": path, "image": crop, "detected": found})
            metadata.append({"row": r + 1, "col": c + 1, "detected": found, "cell_xyxy": [x1, y1, x2, y2], "crop_xyxy": box})

            color = (0, 255, 0) if found else (255, 0, 0)
            cv2.rectangle(debug, (x1, y1), (x2, y2), (150, 100, 0), 1)
            cv2.rectangle(debug, tuple(box[:2]), tuple(box[2:]), color, 1)

            tile = cv2.resize(crop, (128, 128))
            tile = cv2.copyMakeBorder(tile, 22, 0, 0, 0, cv2.BORDER_CONSTANT, value=(40, 40, 40))
            cv2.putText(tile, f"R{r + 1}C{c + 1}", (5, 16), cv2.FONT_HERSHEY_SIMPLEX, 0.45, color, 1)
            tiles.append(tile)

    montage = np.vstack([np.hstack(tiles[i : i + 5]) for i in range(0, 25, 5)])
    cv2.imwrite(os.path.join(output_dir, "crop_debug.png"), debug)
    cv2.imwrite(os.path.join(output_dir, "crop_montage.jpg"), montage)

    with open(os.path.join(output_dir, "crop_boxes.json"), "w", encoding="utf-8") as f:
        json.dump(metadata, f, ensure_ascii=False, indent=2)

    return pills


def analyze_pill(img):
    """Perform geometric analysis to verify pill integrity."""
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    blurred = cv2.GaussianBlur(gray, (5, 5), 0)

    _, thresh = cv2.threshold(blurred, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
    contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

    if not contours:
        return "anormal", 0.0

    img_h, img_w = img.shape[:2]
    center_img = (img_w // 2, img_h // 2)

    valid_contours = []
    for cnt in contours:
        area = cv2.contourArea(cnt)
        if area > 100:
            M = cv2.moments(cnt)
            if M["m00"] != 0:
                cx = int(M["m10"] / M["m00"])
                cy = int(M["m01"] / M["m00"])
                dist = np.hypot(cx - center_img[0], cy - center_img[1])
                valid_contours.append((dist, cnt, area))

    if not valid_contours:
        return "anormal", 0.0

    valid_contours.sort(key=lambda x: x[0])
    _, main_contour, area = valid_contours[0]

    (_, _), radius = cv2.minEnclosingCircle(main_contour)
    ideal_circle_area = np.pi * (radius ** 2)
    area_to_circle_ratio = area / ideal_circle_area if ideal_circle_area > 0 else 0

    x, y, w, h = cv2.boundingRect(main_contour)
    aspect_ratio = min(w, h) / max(w, h) if max(w, h) > 0 else 0
    area_ratio = area / (img_h * img_w)

    checks = [
        area_to_circle_ratio >= 0.72,
        aspect_ratio >= 0.78,
        area_ratio >= 0.08
    ]

    healthy = all(checks)
    label = "healthy" if healthy else "anormal"
    confidence = sum(checks) / len(checks)

    return label, confidence


def send_bad_pills_to_arduino(ser, results):
    """Transmit coordinates of defective items and execute tuned vacuum protocol."""
    bad_pills = [res for res in results if res["status"] == "anormal"]

    if not bad_pills:
        print("[+] No defective pills detected.")
        return

    print(f"\n[+] {len(bad_pills)} defective item(s) found. Starting ejection routine...")

    for pill in bad_pills:
        pos_code = f"Q{pill['row']}{pill['col']}"
        print(f"\n[->] Sending target coordinates: {pos_code}")

        ser.write(f"{pos_code}\n".encode("utf-8"))
        ser.flush()

        # Adjusted positioning delay (4.5s) to ensure arm fully settles before relay triggers
        time.sleep(4.5)

        print("[->] Activating vacuum suction (R)...")
        ser.write(b"R\n")
        ser.flush()

        # Extended vacuum hold duration (2.5s) for reliable suction
        time.sleep(2.5)

        print("[->] Deactivating vacuum suction (O)...")
        ser.write(b"O\n")
        ser.flush()

        # Transition interval before next step
        time.sleep(0.5)

    print("[+] Defective pill extraction cycle complete.\n")


def analyze_sheet(image_path, ser=None):
    """Execute complete analysis pipeline for the current tray image."""
    print(f"Analyzing tray image: {image_path}")

    pills = load_and_crop_pills(image_path)

    results = []
    for pill in pills:
        if not pill.get("detected", True):
            label, confidence = "empty", 0.0
        else:
            label, confidence = analyze_pill(pill["image"])

        results.append(
            {
                "position": f"R{pill['row']}C{pill['col']}",
                "row": pill["row"],
                "col": pill["col"],
                "status": label,
                "confidence": round(confidence * 100, 1),
            }
        )

        print(f"  R{pill['row']}C{pill['col']}: {label} ({confidence * 100:.1f}%)")

    healthy = sum(1 for r in results if r["status"] == "healthy")
    anormal = sum(1 for r in results if r["status"] == "anormal")
    empty = sum(1 for r in results if r["status"] == "empty")

    print(f"\n{'=' * 50}")
    print("INSPECTION SUMMARY:")
    print(f"  Healthy:   {healthy}")
    print(f"  Defective: {anormal}")
    print(f"  Empty:     {empty}")
    print(f"{'=' * 50}")

    if ser is not None and ser.is_open:
        send_bad_pills_to_arduino(ser, results)

    return results


def run_single_inspection():
    """Main execution entry point: Trigger inspection once and return arm home."""
    port = find_arduino_port() or SERIAL_PORT

    if not port:
        print("[!] Serial interface not found.")
        sys.exit(1)

    print(f"[+] Connecting to device on {port}...")

    try:
        ser = serial.Serial(
            port=port,
            baudrate=BAUD_RATE,
            timeout=1,
            write_timeout=2,
        )
    except serial.SerialException as error:
        print(f"[!] Serial communication failure: {error}")
        sys.exit(1)

    time.sleep(2)  # Microcontroller boot delay

    try:
        print("[->] Sending signal 'C' (Move arm to camera frame)...")
        ser.write(GO_TO_CAMERA_SIGNAL)
        ser.flush()

        print("[+] Awaiting READY signal from controller...")
        ready_received = False
        start_wait = time.time()

        while time.time() - start_wait < 30:
            raw_data = ser.readline()
            if not raw_data:
                continue

            line = raw_data.decode("utf-8", errors="ignore").strip()
            if READY_SIGNAL in line:
                print(f"[+] Signal received: {line}")
                ready_received = True
                break

        if not ready_received:
            print("[!] Timeout waiting for READY signal.")
            return

        print("[+] Initiating frame acquisition...")
        saved_path = capture_image_opencv()

        ser.write(ACK_SIGNAL)
        ser.flush()

        analyze_sheet(str(saved_path), ser=ser)

    except Exception as error:
        print(f"[!] System processing error: {error}")

    finally:
        if ser.is_open:
            print("\n[->] Sending signal 'H' (Return arm to Home position)...")
            ser.write(HOME_SIGNAL)
            ser.flush()
            time.sleep(0.5)
            ser.close()
            print("[+] Serial session terminated. Process completed successfully.")


if __name__ == "__main__":
    run_single_inspection()