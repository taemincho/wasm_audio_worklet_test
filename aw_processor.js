class AudioEngineWorkletProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super(options);
    const {
      channelCount = 2, // maximum input channel count
      outputChannelCount = [2],
    } = options;

    this.maxInputCh = channelCount;
    [this.numCh] = outputChannelCount;

    // Audio block size for AudioWorklet is 128
    this.inDataPtr = Module._malloc(128 * 4 * this.maxInputCh);
    this.inbuf = new Float32Array(
      Module.HEAPF32.buffer,
      this.inDataPtr,
      128 * this.maxInputCh
    );
    this.outDataPtr = Module._malloc(128 * 4 * this.numCh);
    this.outbuf = new Float32Array(
      Module.HEAPF32.buffer,
      this.outDataPtr,
      128 * this.numCh
    );

    this.disabled = false;
    this.port.onmessage = (msg) => {
      if ("command" in msg.data) {
        this.call(msg.data, msg.ports);
      } else if (msg.data.message === "disable") {
        this.disabled = true;
      } else {
        console.error(`${msg} is not defined`);
      }
    };

    // Cpp object holder
    this.shared_ptrs = {};

    this.recorder = new Module.Recorder(sampleRate, this.numCh);
  }

  call({ command, args, id }) {
    switch (command) {
      case "RecorderStartRecording":
        {
          this.recorder.startRecording(args.path);
        }
        break;
      case "RecorderStopRecording":
        {
          this.recorder.stopRecording();
        }
        break;
      case "RecorderMonitoring":
        {
          console.log("ID", id);
          this.recorder.monitoring(args.on);
        }
        break;
      case "RecorderGetPos":
        {
          let pos = this.recorder.getPos();
          this.port.postMessage({
            id: id,
            result: pos,
          });
        }
        break;
      case "FSreadFile":
        {
          const finfo = FS.analyzePath(args.filePath, true);
          let data = null;
          if (finfo.exists) {
            try {
              data = FS.readFile(args.filePath);
            } catch (e) {
              console.error(e);
            }
          } else {
            console.error(
              `${args.filePath} doesn't exist, cannot read the file`
            );
          }
          this.port.postMessage({
            id,
            result: data,
          });
        }
        break;
      case "FSexists":
        {
          const finfo = FS.analyzePath(args.path, true);
          this.port.postMessage({
            id,
            result: finfo.exists,
          });
        }
        break;
      case "destroy":
        this.destroy();
        break;
      default:
        console.warn(`${command} is not defined`);
    }
  }

  process(inputs, outputs) {
    // console.log(currentTime);
    if (this.disabled) {
      if (Object.keys(this.shared_ptrs).length !== 0) {
        this.destroy();
      }
      return false;
    }

    if (this.inbuf.length === 0) {
        // when the wasm heap size increases, this javascript array gets detached from the wasm heap
        // and becomes empty. Therefore, it needs to be recreated here.
        this.inbuf = new Float32Array(
        Module.HEAPF32.buffer,
        this.inDataPtr,
        128 * this.maxInputCh
      );
    }
    if (this.outbuf.length === 0) {
        // when the wasm heap size increases, this javascript array gets detached from the wasm heap
        // and becomes empty. Therefore, it needs to be recreated here.
        this.outbuf = new Float32Array(
        Module.HEAPF32.buffer,
        this.outDataPtr,
        128 * this.numCh
      );
    }

    const input = inputs[0];
    const numInputCh = input.length;

    if (numInputCh === 1) {
      const numInputFrame = input[0].length;
      for (let i = 0; i < numInputFrame; i += 1) {
        this.inbuf[i] = input[0][i];
      }
    } else if (numInputCh === 2) {
      // Audio signals in the Web Audio API are not in interleaved format.
      // If you want to enable stereo functionality, you need to convert the signals
      // into interleaved format.
      const numInputFrame = input[0].length;
      for (let i = 0, idx = 0; i < numInputFrame; i += 1) {
        for (let ch = 0; ch < numInputCh; ch += 1) {
          this.inbuf[idx] = input[ch][i];
          idx += 1;
        }
      }
    }

    const output = outputs[0];
    const nFrame = output[0].length;

    this.recorder.process(
      this.inbuf.byteOffset,
      this.outbuf.byteOffset,
      nFrame
    );

    if (this.inbuf.length === 0) {
        // when the wasm heap size increases, this javascript array gets detached from the wasm heap
        // and becomes empty. Therefore, it needs to be recreated here.
        this.inbuf = new Float32Array(
        Module.HEAPF32.buffer,
        this.inDataPtr,
        128 * this.maxInputCh
      );
    }
    if (this.outbuf.length === 0) {
        // when the wasm heap size increases, this javascript array gets detached from the wasm heap
        // and becomes empty. Therefore, it needs to be recreated here.      
        this.outbuf = new Float32Array(
        Module.HEAPF32.buffer,
        this.outDataPtr,
        128 * this.numCh
      );
    }

    // Deinterleave
    for (let i = 0, idx = 0; i < nFrame; i += 1) {
      for (let ch = 0; ch < this.numCh; ch += 1) {
        // add near zero DC offset (0.00001) to prevent audio output rendering mode switching
        output[ch][i] = this.outbuf[idx] + 0.00001;
        idx += 1;
      }
    }

    return true;
  }
} // class AudioEngineWorkletProcessor

registerProcessor("audioengine-worklet-processor", AudioEngineWorkletProcessor);
