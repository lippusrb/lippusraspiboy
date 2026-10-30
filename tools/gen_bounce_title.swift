import AppKit
import CoreText

let outW = 320
let outH = 46
let scale = 3
let W = outW * scale
let H = outH * scale

let bgR: UInt8 = 24
let bgG: UInt8 = 76
let bgB: UInt8 = 40
let cream = NSColor(srgbRed: 245.0 / 255.0, green: 236.0 / 255.0, blue: 220.0 / 255.0, alpha: 1)
let ballFill = NSColor(srgbRed: 206.0 / 255.0, green: 214.0 / 255.0, blue: 72.0 / 255.0, alpha: 1)
let seam = NSColor(srgbRed: 255.0 / 255.0, green: 255.0 / 255.0, blue: 255.0 / 255.0, alpha: 0.9)
let lineColor = NSColor(srgbRed: 245.0 / 255.0, green: 240.0 / 255.0, blue: 228.0 / 255.0, alpha: 1)

func rgb565(_ r: UInt8, _ g: UInt8, _ b: UInt8) -> UInt16 {
    let rr = UInt16(r & 0xF8) << 8
    let gg = UInt16(g & 0xFC) << 3
    let bb = UInt16(b & 0xF8) >> 3
    return rr | gg | bb
}

guard let font = NSFont(name: "Didot-Italic", size: CGFloat(30 * scale)) else {
    fputs("Didot-Italic missing\n", stderr)
    exit(1)
}

let title = NSAttributedString(string: "Lippus Bounce", attributes: [
    .font: font,
    .foregroundColor: cream,
])
let textSize = title.size()
let ballD = CGFloat(11 * scale)
let gap = CGFloat(8 * scale)
let groupW = textSize.width + gap + ballD
let originX = (CGFloat(W) - groupW) / 2.0
let originY = (CGFloat(H) - textSize.height) / 2.0 + CGFloat(scale)

let cs = CGColorSpace(name: CGColorSpace.sRGB)!
guard let ctx = CGContext(
    data: nil,
    width: W,
    height: H,
    bitsPerComponent: 8,
    bytesPerRow: W * 4,
    space: cs,
    bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue
) else {
    fputs("context failed\n", stderr)
    exit(1)
}

let gc = NSGraphicsContext(cgContext: ctx, flipped: true)
NSGraphicsContext.current = gc
NSColor(srgbRed: CGFloat(bgR) / 255.0, green: CGFloat(bgG) / 255.0, blue: CGFloat(bgB) / 255.0, alpha: 1).setFill()
NSBezierPath(rect: NSRect(x: 0, y: 0, width: W, height: H)).fill()

let lineY = originY + textSize.height * 0.58
lineColor.setFill()
NSBezierPath(rect: NSRect(x: 0, y: lineY, width: CGFloat(W), height: CGFloat(scale))).fill()

title.draw(at: NSPoint(x: originX, y: originY))

let ballRect = NSRect(
    x: originX + textSize.width + gap,
    y: lineY - ballD / 2.0,
    width: ballD,
    height: ballD
)
ballFill.setFill()
NSBezierPath(ovalIn: ballRect).fill()
seam.setStroke()
let seamPath = NSBezierPath()
seamPath.lineWidth = CGFloat(scale)
seamPath.move(to: NSPoint(x: ballRect.midX - ballD * 0.28, y: ballRect.minY + ballD * 0.22))
seamPath.curve(
    to: NSPoint(x: ballRect.midX + ballD * 0.28, y: ballRect.maxY - ballD * 0.22),
    controlPoint1: NSPoint(x: ballRect.midX + ballD * 0.05, y: ballRect.midY - ballD * 0.05),
    controlPoint2: NSPoint(x: ballRect.midX - ballD * 0.05, y: ballRect.midY + ballD * 0.05)
)
seamPath.stroke()

guard let data = ctx.data else {
    fputs("no pixels\n", stderr)
    exit(1)
}
let pixels = data.bindMemory(to: UInt8.self, capacity: W * H * 4)
var small = [UInt8](repeating: 0, count: outW * outH * 3)
for y in 0..<outH {
    for x in 0..<outW {
        var r = 0
        var g = 0
        var b = 0
        for oy in 0..<scale {
            for ox in 0..<scale {
                let sx = x * scale + ox
                let sy = y * scale + oy
                let i = (sy * W + sx) * 4
                r += Int(pixels[i])
                g += Int(pixels[i + 1])
                b += Int(pixels[i + 2])
            }
        }
        let n = scale * scale
        let o = ((outH - 1 - y) * outW + x) * 3
        small[o] = UInt8(r / n)
        small[o + 1] = UInt8(g / n)
        small[o + 2] = UInt8(b / n)
    }
}

let root = URL(fileURLWithPath: CommandLine.arguments[1])
let pngURL = root.appendingPathComponent("games/lippus_bounce/title.png")
let hdrURL = root.appendingPathComponent("games/lippus_bounce/title.h")

let rep = NSBitmapImageRep(
    bitmapDataPlanes: nil,
    pixelsWide: outW,
    pixelsHigh: outH,
    bitsPerSample: 8,
    samplesPerPixel: 3,
    hasAlpha: false,
    isPlanar: false,
    colorSpaceName: .deviceRGB,
    bytesPerRow: outW * 3,
    bitsPerPixel: 24
)!
for i in 0..<small.count {
    rep.bitmapData![i] = small[i]
}
let png = rep.representation(using: .png, properties: [:])!
try png.write(to: pngURL)

var lines: [String] = []
lines.append("/* Generato da tools/gen_bounce_title.swift — non editare a mano. */")
lines.append("#pragma once")
lines.append("#include <stdint.h>")
lines.append("")
lines.append("#define TITLE_W \(outW)")
lines.append("#define TITLE_H \(outH)")
lines.append("")
lines.append("static const uint16_t k_title[TITLE_W * TITLE_H] = {")
var row: [String] = []
for i in 0..<(outW * outH) {
    let r = small[i * 3]
    let g = small[i * 3 + 1]
    let b = small[i * 3 + 2]
    row.append(String(format: "0x%04X", rgb565(r, g, b)))
    if row.count == 12 || i + 1 == outW * outH {
        lines.append("    " + row.joined(separator: ", ") + ",")
        row.removeAll()
    }
}
lines.append("};")
lines.append("")
try lines.joined(separator: "\n").write(to: hdrURL, atomically: true, encoding: .utf8)
print("wrote \(outW)x\(outH) font \(font.fontName)")
