# ffmpeg-legofylter

As the project title says, this is an [FFmpeg](https://www.ffmpeg.org/) filter to transform individual frames of a video into LEGO-like images.

This was done as an experiment in a larger project of mine to see how much faster it is to process images directly inside FFmpeg. I also wanted to see how complicated it is to write FFmpeg filters. To my surprise it wasn't that much faster than using a pipe and processing the individual frames in Golang. I assume that makes sense since it was done in software and not hardware. A 60 FPS 1920x1080, 3 minute long AVC video was rendered with 0.246x speed while using this filter on a Ryzen 7 5700U whereas Golang speeds were slightly slower, but the difference was negligible and the latter allows for more flexibility and you don't need to compile your own FFmpeg.

The code is definitely not production ready as it didn't prove its usefulness in my project. With little work, you can adjust it to your needs however.

Some parts could be optimized further, but I am not too familiar with the FFmpeg API, all the code was based on reading and understanding other existing filters.

I don't intend to support this project therefore it goes out in an archived state and no further development will be done. If you want to use it, feel free to do so, but I won't be able to help you with any issues you might encounter.