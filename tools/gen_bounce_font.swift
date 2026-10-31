import AppKit
import CoreText

let scale = 3
let chars = Array("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789, ")
let root = URL(fileURLWithPath: CommandLine.arguments[1])

struct Glyph {
    var dx: Int
    var dy: Int
    var w: Int
    var h: Int
    var adv: Int
    var alpha: [UInt8]
}

func makeFont(_ pt: Int) -> NSFont {
    guard let font = NSFont(name: "Didot-Italic", size: CGFloat(pt * scale)) else {
        fputs("Didot-Italic missing\n", stderr)
        exit(1)
    }
    return font
}

func renderSet(_ pt: Int) -> (glyphs: [Glyph], lineH: Int) {
    let font = makeFont(pt)
    let ascent = font.ascender
    let descent = -font.descender
    let lineH = Int((ascent + descent) / CGFloat(scale) + 0.999)
    let canvasH = lineH * scale
    var glyphs: [Glyph] = []

    for ch in chars {
        let text = NSAttributedString(string: String(ch), attributes: [
            .font: font,
            .foregroundColor: NSColor.white,
        ])
        let line = CTLineCreateWithAttributedString(text)
        var a: CGFloat = 0
        var d: CGFloat = 0
        var leading: CGFloat = 0
        let advance = CTLineGetTypographicBounds(line, &a, &d, &leading)
        let margin = 24 * scale
        let canvasW = Int(advance) + margin * 2 + 8 * scale
        let cs = CGColorSpace(name: CGColorSpace.sRGB)!
        guard let ctx = CGContext(
            data: nil,
            width: canvasW,
            height: canvasH,
            bitsPerComponent: 8,
            bytesPerRow: canvasW * 4,
            space: cs,
            bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue
        ) else {
            fputs("context failed\n", stderr)
            exit(1)
        }
        ctx.clear(CGRect(x: 0, y: 0, width: canvasW, height: canvasH))
        let baseline = descent
        ctx.textPosition = CGPoint(x: CGFloat(margin), y: baseline)
        CTLineDraw(line, ctx)

        let src = ctx.data!.bindMemory(to: UInt8.self, capacity: canvasW * canvasH * 4)
        var alpha = [UInt8](repeating: 0, count: lineH * (canvasW / scale + 1))
        let outW = canvasW / scale
        for y in 0..<lineH {
            for x in 0..<outW {
                var sum = 0
                for oy in 0..<scale {
                    for ox in 0..<scale {
                        let sx = x * scale + ox
                        let sy = y * scale + oy
                        if sx >= canvasW || sy >= canvasH {
                            continue
                        }
                        let i = (sy * canvasW + sx) * 4
                        sum += Int(src[i + 3])
                    }
                }
                alpha[y * outW + x] = UInt8(sum / (scale * scale))
            }
        }

        var minX = outW
        var minY = lineH
        var maxX = 0
        var maxY = 0
        var any = false
        for y in 0..<lineH {
            for x in 0..<outW {
                if alpha[y * outW + x] > 12 {
                    any = true
                    if (x < minX) { minX = x }
                    if (y < minY) { minY = y }
                    if (x + 1 > maxX) { maxX = x + 1 }
                    if (y + 1 > maxY) { maxY = y + 1 }
                }
            }
        }
        let adv = max(1, Int((advance / CGFloat(scale)).rounded()))
        let pen = margin / scale
        if !any {
            glyphs.append(Glyph(dx: 0, dy: 0, w: 0, h: 0, adv: adv, alpha: []))
            continue
        }
        let w = maxX - minX
        let h = maxY - minY
        var ink = [UInt8](repeating: 0, count: w * h)
        for y in 0..<h {
            for x in 0..<w {
                ink[y * w + x] = alpha[(minY + y) * outW + (minX + x)]
            }
        }
        glyphs.append(Glyph(dx: minX - pen, dy: minY, w: w, h: h, adv: adv, alpha: ink))
    }
    return (glyphs, lineH)
}

func appendGlyphs(_ lines: inout [String], _ name: String, _ glyphs: [Glyph]) {
    var blob: [UInt8] = []
    var offs: [Int] = []
    for g in glyphs {
        offs.append(blob.count)
        blob.append(contentsOf: g.alpha)
    }
    lines.append("static const court_glyph_t \(name)[COURT_N] = {")
    for (i, g) in glyphs.enumerated() {
        let ch = chars[i] == " " ? "sp" : String(chars[i])
        lines.append("    { \(g.dx), \(g.dy), \(g.w), \(g.h), \(g.adv), \(offs[i]) }, /* \(ch) */")
    }
    lines.append("};")
    lines.append("static const uint8_t \(name)_px[\(max(blob.count, 1))] = {")
    if blob.isEmpty {
        lines.append("    0,")
    } else {
        var row: [String] = []
        for (i, v) in blob.enumerated() {
            row.append(String(v))
            if row.count == 24 || i + 1 == blob.count {
                lines.append("    " + row.joined(separator: ", ") + ",")
                row.removeAll()
            }
        }
    }
    lines.append("};")
    lines.append("")
}

let sm = renderSet(12)
let lg = renderSet(18)

var lines: [String] = []
lines.append("/* Generato da tools/gen_bounce_font.swift — non editare a mano. */")
lines.append("#pragma once")
lines.append("#include <stdint.h>")
lines.append("")
lines.append("typedef struct {")
lines.append("    int8_t dx;")
lines.append("    int8_t dy;")
lines.append("    uint8_t w;")
lines.append("    uint8_t h;")
lines.append("    uint8_t adv;")
lines.append("    uint16_t off;")
lines.append("} court_glyph_t;")
lines.append("")
lines.append("#define COURT_N 38")
lines.append("#define COURT_SM 0")
lines.append("#define COURT_LG 1")
lines.append("#define COURT_SM_H \(sm.lineH)")
lines.append("#define COURT_LG_H \(lg.lineH)")
lines.append("")
appendGlyphs(&lines, "k_court_sm", sm.glyphs)
appendGlyphs(&lines, "k_court_lg", lg.glyphs)

let hdr = root.appendingPathComponent("games/lippus_bounce/court_font.h")
try lines.joined(separator: "\n").write(to: hdr, atomically: true, encoding: .utf8)

func widthOf(_ glyphs: [Glyph], _ s: String) -> Int {
    var x = 0
    var lo = 0
    var hi = 0
    var any = false
    for ch in s {
        let i: Int
        if ch >= "A" && ch <= "Z" {
            i = Int(ch.asciiValue! - 65)
        } else if ch >= "0" && ch <= "9" {
            i = 26 + Int(ch.asciiValue! - 48)
        } else if ch == "," {
            i = 36
        } else {
            i = 37
        }
        let g = glyphs[i]
        if g.w > 0 {
            let l = x + g.dx
            let r = l + g.w
            if !any || l < lo { lo = l }
            if !any || r > hi { hi = r }
            any = true
        }
        x += g.adv
    }
    return any ? hi - lo : x
}

let samples = [
    "UP AND DOWN MOVE THE PADDLE",
    "THE EDGE CHANGES THE ANGLE",
    "0  15  30  40, THEN DEUCE AND AD",
    "ONE LOST GAME ENDS THE MATCH",
    "PRESS START",
    "NEW HIGH SCORE",
    "ADVANTAGE YOU",
    "YOU WIN",
    "GAME OVER",
    "MOVE TO AIM",
    "10 ABC 3000",
]
print("sm line \(sm.lineH) lg line \(lg.lineH)")
for s in samples {
    print("sm \(widthOf(sm.glyphs, s)) lg \(widthOf(lg.glyphs, s))  \(s)")
}

func paint(_ glyphs: [Glyph], _ s: String, _ y: Int, _ rgb: inout [UInt8], _ W: Int) {
    let creamR = 245
    let creamG = 236
    let creamB = 220
    let bgR = 24
    let bgG = 76
    let bgB = 40
    var x = 8
    for ch in s {
        let i: Int
        if ch >= "A" && ch <= "Z" { i = Int(ch.asciiValue! - 65) }
        else if ch >= "0" && ch <= "9" { i = 26 + Int(ch.asciiValue! - 48) }
        else if ch == "," { i = 36 }
        else { i = 37 }
        let g = glyphs[i]
        for row in 0..<g.h {
            for col in 0..<g.w {
                let a = Int(g.alpha[row * g.w + col])
                let px = x + g.dx + col
                let py = y + g.dy + row
                if a < 8 || px < 0 || py < 0 || px >= W || py >= 80 { continue }
                let o = (py * W + px) * 3
                rgb[o] = UInt8((bgR * (255 - a) + creamR * a) / 255)
                rgb[o + 1] = UInt8((bgG * (255 - a) + creamG * a) / 255)
                rgb[o + 2] = UInt8((bgB * (255 - a) + creamB * a) / 255)
            }
        }
        x += g.adv
    }
}

let pw = 340
let ph = 80
var rgb = [UInt8](repeating: 0, count: pw * ph * 3)
for i in 0..<(pw * ph) {
    rgb[i * 3] = 24
    rgb[i * 3 + 1] = 76
    rgb[i * 3 + 2] = 40
}
paint(lg.glyphs, "YOU WIN", 4, &rgb, pw)
paint(sm.glyphs, "ONE LOST GAME ENDS THE MATCH", 40, &rgb, pw)
let rep = NSBitmapImageRep(
    bitmapDataPlanes: nil,
    pixelsWide: pw,
    pixelsHigh: ph,
    bitsPerSample: 8,
    samplesPerPixel: 3,
    hasAlpha: false,
    isPlanar: false,
    colorSpaceName: .deviceRGB,
    bytesPerRow: pw * 3,
    bitsPerPixel: 24
)!
for i in 0..<rgb.count { rep.bitmapData![i] = rgb[i] }
let png = rep.representation(using: .png, properties: [:])!
try png.write(to: root.appendingPathComponent("games/lippus_bounce/font_preview.png"))
print("wrote court_font.h")
