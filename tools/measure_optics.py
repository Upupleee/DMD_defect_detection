# -*- coding: utf-8 -*-
"""
DMD 检测系统光学参数复算脚本（只读，不改任何原始图像）。

用 testimgs/ 下的亮场实拍图拟合 DMD 有效区旋转矩形，结合器件手册参数
反推像素当量 s 与镜头倍率 beta，并用栅格频谱做独立交叉验证。

运行方式（本机终端没有 python 命令，必须用绝对路径）：
"C:/Users/Administrator/.workbuddy-ai/binaries/python/versions/3.13.12/python.exe" measure_optics.py

依赖：numpy, opencv-python, Pillow
手册参数来源：TI DLP9500 datasheet、Gpixel GMAX3265 datasheet
"""
import os
import glob
import struct
import numpy as np
import cv2

# ---------------- 配置 ----------------
ROOT = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))), "DMD坏点检测", "testimgs")

DMD_COLS, DMD_ROWS = 1920, 1080
D_MIRROR_UM = 10.8   # DLP9500BFLN 微镜间距
P_PIXEL_UM = 3.2     # GMAX3265 像元尺寸
SENSOR_W, SENSOR_H = 9344, 7000
DOWN = 4             # 轮廓检测下采样倍率


def load_gray_bmp(path):
    """按字节索引读取 8bit 灰度 BMP，处理 4 字节行对齐与 bottom-up 存储。"""
    with open(path, "rb") as f:
        hdr = f.read(54)
    if hdr[:2] != b"BM":
        raise ValueError("不是 BMP 文件: %s" % path)
    off = struct.unpack_from("<I", hdr, 10)[0]
    w = struct.unpack_from("<i", hdr, 18)[0]
    h = struct.unpack_from("<i", hdr, 22)[0]
    bpp = struct.unpack_from("<H", hdr, 28)[0]
    comp = struct.unpack_from("<I", hdr, 30)[0]
    if bpp != 8 or comp != 0:
        raise ValueError("仅支持无压缩 8bpp 灰度 BMP: %s" % path)
    stride = ((w + 3) // 4) * 4
    buf = np.fromfile(path, dtype=np.uint8, offset=off, count=stride * h)
    img = buf.reshape(h, stride)[:, :w].copy()
    return np.flipud(img), (w, h, bpp, off)


def measure_dmd_rect(path):
    """最大亮连通域的最小外接旋转矩形，返回 (宽, 高, 角度)。"""
    img, _ = load_gray_bmp(path)
    small = img[::DOWN, ::DOWN]
    _, bw = cv2.threshold(small, 100, 255, cv2.THRESH_BINARY)
    bw = cv2.morphologyEx(bw, cv2.MORPH_CLOSE, np.ones((9, 9), np.uint8))
    cnts, _ = cv2.findContours(bw, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if not cnts:
        return None
    c = max(cnts, key=cv2.contourArea)
    (_, _), (rw, rh), ang = cv2.minAreaRect(c)
    return max(rw, rh) * DOWN, min(rw, rh) * DOWN, ang


def grid_period_check(path):
    """亮区内部高频栅格主周期，用于独立验证像素当量。"""
    img, _ = load_gray_bmp(path)
    roi = img[3300:4500, 3500:4700].astype(np.float32)
    roi = roi - cv2.GaussianBlur(roi, (0, 0), 3)
    periods = []
    for sig in (roi.mean(axis=0), roi.mean(axis=1)):
        sig = sig - sig.mean()
        sp = np.abs(np.fft.rfft(sig * np.hanning(len(sig))))
        freq = np.fft.rfftfreq(len(sig))
        band = np.where((freq > 1 / 12.0) & (freq < 1 / 2.0))[0]
        best = band[np.argmax(sp[band])]
        periods.append(1.0 / freq[best])
    return periods


def main():
    files = sorted(glob.glob(os.path.join(ROOT, "*.bmp")))
    if not files:
        raise SystemExit("未找到实拍图，检查 ROOT: %s" % ROOT)

    all_rows = []
    for f in files:
        r = measure_dmd_rect(f)
        if r is None:
            print("  跳过（无亮连通域）: %s" % os.path.basename(f))
            continue
        W, H, ang = r
        all_rows.append((W, H, os.path.basename(f)))
        print("  %-44s W=%7.1f H=%7.1f W/H=%.4f" %
              (os.path.basename(f), W, H, W / H))

    # 只保留严格 16:9 的样本：宽高比偏离说明轮廓被亮度渐变拉宽
    strict = [r for r in all_rows if abs(r[0] / r[1] - DMD_COLS / DMD_ROWS) < 0.003]
    if not strict:
        raise SystemExit("没有符合 16:9 的样本，无法定标")
    W = float(np.mean([r[0] for r in strict]))
    H = float(np.mean([r[1] for r in strict]))
    print("\n严格 16:9 子集: %d/%d 张" % (len(strict), len(all_rows)))

    sx, sy = W / DMD_COLS, H / DMD_ROWS
    dmd_w = DMD_COLS * D_MIRROR_UM / 1000.0
    dmd_h = DMD_ROWS * D_MIRROR_UM / 1000.0
    img_w = W * P_PIXEL_UM / 1000.0
    img_h = H * P_PIXEL_UM / 1000.0
    bx, by = img_w / dmd_w, img_h / dmd_h

    print("  DMD 像      : W=%.1f px  H=%.1f px" % (W, H))
    print("  像素当量    : s_x=%.4f  s_y=%.4f px/微镜" % (sx, sy))
    print("  DMD 物理    : %.4f x %.4f mm" % (dmd_w, dmd_h))
    print("  DMD 像物理  : %.4f x %.4f mm" % (img_w, img_h))
    print("  beta_x=%.4f  beta_y=%.4f  -> beta=%.4f" % (bx, by, (bx + by) / 2))
    print("  靶面填充    : 宽 %.1f%%  高 %.1f%%" %
          (img_w / (SENSOR_W * P_PIXEL_UM / 1000.0) * 100,
           img_h / (SENSOR_H * P_PIXEL_UM / 1000.0) * 100))

    # 独立交叉验证：栅格主周期应接近 s
    print("\n栅格频谱交叉验证（应接近 s = %.3f px）:" % sx)
    for f in files:
        p = grid_period_check(f)
        print("  %-44s col=%.2f px  row=%.2f px" %
              (os.path.basename(f), p[0], p[1]))
        break

    print("\n注意：若相机启用了 binning 或数字缩放，P_PIXEL_UM 不再是 3.2，"
          "beta 需按比例修正。该项未验证。")


if __name__ == "__main__":
    main()
