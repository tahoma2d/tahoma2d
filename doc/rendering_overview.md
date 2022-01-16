# Rendering in Tahoma2D

Understanding the rendering code can be challenging in Tahoma2D

When a render, fast render, or preview command is given, it is handled in rendercommand.cpp

### In rendercommand.cpp:
- The file destination is verified.
- The output or preview settings are retrieved
- The scene background color is adjusted if the exported file type does not support transparency.
- The fx stack for each frame is retrieved, including adding an additional over fx to lay the frame over the background color.
- The process is then handed off to a movierenderer or a multimediarenderer.

A movierenderer exports standard image sequences or video formats.
(movierenderer.cpp)

A multimediarenderer exports each layer of a frame individually for compositing elsewhere later.
(multimediarenderer.cpp)

## This document will focus on what happens with a movierenderer.

### In movierenderer.cpp:
- A movierenderer class receives the settings from the rendercommand
- This class receives the list of frames to be rendered with their fx stack
- Passes this information on to an Imp class which subclasses TrenderPort
    - The Imp communicates with the renderer and handles post rendering tasks, such as sending rendered frames to the level writer, creating the soundtrack, and adding a clapper board.
- The Imp deletes old files if needed.
- The Imp sets the output size based on the shrink setting from the output settings.
- The Imp set up the Level Updater and Writer which will handled the frames when rendering is done.
- Once all setup is done, the movierenderer calls startRendering.

### The process is then handed off to trenderer.cpp

The TRenderer assigns a render id and tells the TRendererStartInvoker to emit start render.

A TRendererStartInvoker is a middle class that emits a startRender message to itself(?) and tells the renderer to begin (startRendering()).

### In TRendererImp::startRendering():
- The output area is setup
- An Individual RenderTask is setup for each frame
- The render tasks are launched

### RenderTask::run() is where the frame renders begin.
- An empty raster tile is retrieved
- The time is computed in compute()
  - Compute is handled by TRasterFx::compute() - see below
- A notification that the frame is complete is sent

### in TRasterFx::compute()
- The tile is adjusted to make sure that it is not at a fractional position
- An alias string is built which contains the fx stack and parameters.
- The tile and relevant info is then passed to a ResourceBuilder 

### The ResourceBuilder:
- runs build
- checks to see if the resource already exists
- if it doesn't compute() is called
- Compute calls doCompute on the fx
- This fx will call compute on any needed info it needs to do it's processing.
- This can continue for as many fx need info/data.
- Once all fx for a frame have been processed, a final tile should be created and passed back.

## The MovieRenderer::Imp will then get the final frames and tell the level writer to save them.






