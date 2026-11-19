import AppKit
import ImageIO

/// Launcher art. A player's own `hero.png`, `logo.png` or `icon.png` in
/// `~/Library/Application Support/AzerothCore/art` wins over the bundled original art.
/// A file that is missing, unreadable or oversized is ignored.
enum Art {
    private static let maxFileBytes = 32 * 1024 * 1024
    private static let maxSide = 8192

    static func image(_ name: String, maxPixel: Int) -> NSImage? {
        let custom = URL(fileURLWithPath: artDir()).appendingPathComponent("\(name).png")
        if let img = load(custom, maxPixel: maxPixel) { return img }
        if let bundled = Bundle.main.url(forResource: name, withExtension: "png"), let img = load(bundled, maxPixel: maxPixel) { return img }
        return nil
    }

    /// Reads the size from the header first, then decodes a downsampled copy, so a huge file never gets fully decoded.
    private static func load(_ url: URL, maxPixel: Int) -> NSImage? {
        guard let size = try? url.resourceValues(forKeys: [.fileSizeKey]).fileSize, size > 0, size <= maxFileBytes,
              let src = CGImageSourceCreateWithURL(url as CFURL, nil),
              let props = CGImageSourceCopyPropertiesAtIndex(src, 0, nil) as? [CFString: Any],
              let w = props[kCGImagePropertyPixelWidth] as? Int, let h = props[kCGImagePropertyPixelHeight] as? Int,
              w > 0, h > 0, w <= maxSide, h <= maxSide else { return nil }
        let opts: [CFString: Any] = [
            kCGImageSourceCreateThumbnailFromImageAlways: true,
            kCGImageSourceCreateThumbnailWithTransform: true,
            kCGImageSourceThumbnailMaxPixelSize: maxPixel,
        ]
        guard let cg = CGImageSourceCreateThumbnailAtIndex(src, 0, opts as CFDictionary) else { return nil }
        return NSImage(cgImage: cg, size: NSSize(width: cg.width, height: cg.height))
    }
}
