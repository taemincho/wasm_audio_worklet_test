export class AudioEngineWorkletNode extends AudioWorkletNode {
  constructor(context) {
    super(context, "audioengine-worklet-processor", {
      channelCount: 2, // allow input channels upto stereo
      channelCountMode: "clamped-max", // if the number of input channels exceeds 2, it will take the first two.
      outputChannelCount: [2], // the output channel must be a stereo
    });

    this.port.onmessage = (msg) => {
      // console.log(msg)
    };
  }

  _commandWithReturn(command, args) {
    return new Promise((resolve) => {
      const id = crypto.randomUUID();
      const fn = (event) => {
        if (event.data.id === id) {
          this.port.removeEventListener("message", fn);
          resolve(event.data.result);
          event.stopPropagation();
        }
      };
      this.port.addEventListener("message", fn);
      this.port.postMessage({
        command,
        args,
        id,
      });
    });
  }

  _commandWithoutReturn(command, args) {
    this.port.postMessage({
      command,
      args,
    });
  }

  destroy() {
    this._commandWithoutReturn("destroy", {});
  }

  // #region ****** File System APIs **********
  FS = {
    readFile: (filePath) =>
      this._commandWithReturn("FSreadFile", { filePath }),
    exists: (path) => this._commandWithReturn("FSexists", { path }),
  };

  Recorder = {
    startRecording: (path) =>
      this._commandWithoutReturn("RecorderStartRecording", { path }),
    stopRecording: () =>
      this._commandWithoutReturn("RecorderStopRecording", {}),
    monitoring: (on) =>
      this._commandWithoutReturn("RecorderMonitoring", { on }),
    getPos: () => this._commandWithReturn("RecorderGetPos", {}),
  };
}
