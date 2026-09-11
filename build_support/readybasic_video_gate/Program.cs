using ViceTasks.Ultimate;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;

// Deliberately never read C64 RAM, screen RAM, or machine state here. REST
// memory access can hang Ultimate IEC transfers. Only video-stream control
// and UDP frames are used until the cyan READY letters visibly animate.
var output = Path.GetFullPath(args[0]);
Directory.CreateDirectory(output);
using var client = new UltimateClient(new UltimateClientOptions());
var started = DateTime.UtcNow;
bool[]? previous = null;
var consecutive = 0;
var frameNumber = 0;
while ((DateTime.UtcNow - started).TotalSeconds < 600)
{
    using var frame = await client.CaptureVideoFrameAsync(port:11003);
    var mask = new bool[frame.Width * frame.Height];
    var count = 0;
    var changed = 0;
    var backdrop = 0;
    // PAL native video coordinates, covering the five cyan sprite glyphs.
    for (var y = 45; y < Math.Min(150, frame.Height); y++)
    for (var x = 35; x < Math.Min(350, frame.Width); x++)
    {
        Rgba32 pixel = frame[x,y];
        var cyan = pixel.G > 110 && pixel.B > 110 && pixel.R * 5 < Math.Min(pixel.G, pixel.B) * 4;
        var index = y * frame.Width + x;
        mask[index] = cyan;
        if (cyan) count++;
        if (previous != null && previous[index] != cyan) changed++;
    }
    // Reject the temporary generated sprite patterns on the cleared screen
    // before SPRFILE/MCFILE. The red space backdrop must also be visible.
    for (var y = 150; y < Math.Min(230, frame.Height); y++)
    for (var x = 35; x < Math.Min(350, frame.Width); x++)
    {
        var pixel = frame[x,y];
        if (pixel.R > 60 && pixel.R * 10 > pixel.G * 13 && pixel.R * 10 > pixel.B * 13)
            backdrop++;
    }
    var complete = count > 200 && backdrop > 500;
    consecutive = complete && previous != null && changed > 40 ? consecutive + 1 : 0;
    previous = complete ? mask : null;
    await frame.SaveAsPngAsync(Path.Combine(output, "latest.png"));
    Console.WriteLine($"{(DateTime.UtcNow-started).TotalSeconds:F1}s cyan={count} backdrop={backdrop} changed={changed} moving={consecutive}");
    if (consecutive >= 3)
    {
        await frame.SaveAsPngAsync(Path.Combine(output, "motion-ready.png"));
        Console.WriteLine("VIDEO_MOTION_READY: safe to begin memory inspection.");
        return;
    }
    if (++frameNumber % 20 == 0)
        await frame.SaveAsPngAsync(Path.Combine(output, $"loading-{frameNumber:D4}.png"));
    await Task.Delay(350);
}
Console.Error.WriteLine("No confirmed sprite motion; refusing to inspect memory.");
Environment.ExitCode = 1;
