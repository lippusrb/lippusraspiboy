import AppKit

let scale = 3
let fontPx = 18
let pad = 4
let ballPx = 7
let gapPx = 8

let bgR: UInt8 = 24
let bgG: UInt8 = 76
let bgB: UInt8 = 40
let cream = NSColor(srgbRed: 245.0 / 255.0, green: 236.0 / 255.0, blue: 220.0 / 255.0, alpha: 1)
let ballFill = NSColor(srgbRed: 206.0 / 255.0, green: 214.0 / 255.0, blue: 72.0 / 255.0, alpha: 1)
let seam = NSColor(srgbRed: 255.0 / 255.0, green: 255.0 / 255.0, blue: 255.0 / 255.0, alpha: 0.95)
let bg = NSColor(srgbRed: CGFloat(bgR) / 255.0, green: CGFloat(bgG) / 255.0, blue: CGFloat(bgB) / 255.0, alpha: 1)

func rgb565(_ r: UInt8, _ g: UInt8, _ b: UInt8) -> UInt16 {
    let rr = UInt16(r & 0xF8) << 8
    let gg = UInt16(g & 0xFC) << 3
    let bb = UInt16(b & 0xF8) >> 3
    return rr | gg | bb
}

func downsample(_ pixels: UnsafeMutableRawPointer, _ srcW: Int, _ srcH: Int, _ outW: Int, _ outH: Int) -> [UInt8] {
    let src = pixels.bindMemory(to: UInt8.self, capacity: srcW * srcH * 4)
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
                    let i = (sy * srcW + sx) * 4
                    r += Int(src[i])
                    g += Int(src[i + 1])
                    b += Int(src[i + 2])
                }
            }
            let n = scale * scale
            let o = ((outH - 1 - y) * outW + x) * 3
            small[o] = UInt8(r / n)
            small[o + 1] = UInt8(g / n)
            small[o + 2] = UInt8(b / n)
        }
    }
    return small
}

func makeContext(_ w: Int, _ h: Int) -> CGContext {
    let cs = CGColorSpace(name: CGColorSpace.sRGB)!
    guard let ctx = CGContext(
        data: nil,
        width: w,
        height: h,
        bitsPerComponent: 8,
        bytesPerRow: w * 4,
        space: cs,
        bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue
    ) else {
        fputs("context failed\n", stderr)
        exit(1)
    }
    return ctx
}

func writePNG(_ small: [UInt8], _ w: Int, _ h: Int, _ url: URL) {
    let rep = NSBitmapImageRep(
        bitmapDataPlanes: nil,
        pixelsWide: w,
        pixelsHigh: h,
        bitsPerSample: 8,
        samplesPerPixel: 3,
        hasAlpha: false,
        isPlanar: false,
        colorSpaceName: .deviceRGB,
        bytesPerRow: w * 3,
        bitsPerPixel: 24
    )!
    for i in 0..<small.count {
        rep.bitmapData![i] = small[i]
    }
    let png = rep.representation(using: .png, properties: [:])!
    try! png.write(to: url)
}

func append565(_ lines: inout [String], _ decl: String, _ small: [UInt8], _ count: Int) {
    lines.append("static const uint16_t \(decl) = {")
    var row: [String] = []
    for i in 0..<count {
        let r = small[i * 3]
        let g = small[i * 3 + 1]
        let b = small[i * 3 + 2]
        row.append(String(format: "0x%04X", rgb565(r, g, b)))
        if row.count == 12 || i + 1 == count {
            lines.append("    " + row.joined(separator: ", ") + ",")
            row.removeAll()
        }
    }
    lines.append("};")
    lines.append("")
}

guard let font = NSFont(name: "Didot-Italic", size: CGFloat(fontPx * scale)) else {
    fputs("Didot-Italic missing\n", stderr)
    exit(1)
}

let title = NSAttributedString(string: "Lippus Bounce", attributes: [
    .font: font,
    .foregroundColor: cream,
])
let textSize = title.size()
let outW = 320
let textH = Int((textSize.height / CGFloat(scale)).rounded(.up))
let outH = textH + pad * 2
let W = outW * scale
let H = outH * scale

let ballD = CGFloat(ballPx * scale)
let gap = CGFloat(gapPx * scale)
let groupW = textSize.width + gap + ballD
let originX = (CGFloat(W) - groupW) / 2.0
let originY = (CGFloat(H) - textSize.height) / 2.0

let ctx = makeContext(W, H)
NSGraphicsContext.current = NSGraphicsContext(cgContext: ctx, flipped: true)
bg.setFill()
NSBezierPath(rect: NSRect(x: 0, y: 0, width: W, height: H)).fill()
title.draw(at: NSPoint(x: originX, y: originY))

guard let data = ctx.data else {
    fputs("no pixels\n", stderr)
    exit(1)
}
let word = downsample(data, W, H, outW, outH)

let ballX = Int(((originX + textSize.width + gap) / CGFloat(scale)).rounded())
let ballY = Int(((originY + textSize.height * 0.46 - ballD / 2.0) / CGFloat(scale)).rounded())
let inkL = Int((originX / CGFloat(scale)).rounded())
let inkR = Int(((originX + textSize.width) / CGFloat(scale)).rounded())
let arc = (ballY - 1) + (ballPx + 2) + 3

let spr = ballPx + 2
let SW = spr * scale
let ballCtx = makeContext(SW, SW)
NSGraphicsContext.current = NSGraphicsContext(cgContext: ballCtx, flipped: true)
bg.setFill()
NSBezierPath(rect: NSRect(x: 0, y: 0, width: SW, height: SW)).fill()
let inset = CGFloat(scale)
let ballRect = NSRect(x: inset, y: inset, width: ballD, height: ballD)
ballFill.setFill()
NSBezierPath(ovalIn: ballRect).fill()
seam.setStroke()
let seamPath = NSBezierPath()
seamPath.lineWidth = CGFloat(scale)
seamPath.move(to: NSPoint(x: ballRect.midX - ballD * 0.22, y: ballRect.minY + ballD * 0.28))
seamPath.curve(
    to: NSPoint(x: ballRect.midX + ballD * 0.22, y: ballRect.maxY - ballD * 0.28),
    controlPoint1: NSPoint(x: ballRect.midX + ballD * 0.18, y: ballRect.midY - ballD * 0.02),
    controlPoint2: NSPoint(x: ballRect.midX - ballD * 0.18, y: ballRect.midY + ballD * 0.02)
)
seamPath.stroke()
guard let ballData = ballCtx.data else {
    fputs("no ball pixels\n", stderr)
    exit(1)
}
let ball = downsample(ballData, SW, SW, spr, spr)

let root = URL(fileURLWithPath: CommandLine.arguments[1])
writePNG(word, outW, outH, root.appendingPathComponent("games/lippus_bounce/title.png"))
writePNG(ball, spr, spr, root.appendingPathComponent("games/lippus_bounce/ball.png"))

var lines: [String] = []
lines.append("/* Generato da tools/gen_bounce_title.swift — non editare a mano. */")
lines.append("#pragma once")
lines.append("#include <stdint.h>")
lines.append("")
lines.append("#define TITLE_W \(outW)")
lines.append("#define TITLE_H \(outH)")
lines.append("#define TITLE_BALL_X \(ballX - 1)")
lines.append("#define TITLE_BALL_Y \(ballY - 1)")
lines.append("#define TITLE_INK_L \(inkL)")
lines.append("#define TITLE_INK_R \(inkR)")
lines.append("#define TITLE_ARC \(arc)")
lines.append("#define BALL_W \(spr)")
lines.append("#define BALL_H \(spr)")
lines.append("#define TITLE_BALL_KEY 0x1A65")
lines.append("")
append565(&lines, "k_title[TITLE_W * TITLE_H]", word, outW * outH)
append565(&lines, "k_ball[BALL_W * BALL_H]", ball, spr * spr)

let hdrURL = root.appendingPathComponent("games/lippus_bounce/title.h")
try lines.joined(separator: "\n").write(to: hdrURL, atomically: true, encoding: .utf8)
print("wrote \(outW)x\(outH) ink \(inkL)..\(inkR) ball \(ballX),\(ballY) arc \(arc)")
