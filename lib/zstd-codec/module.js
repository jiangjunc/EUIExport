// REF: https://stackoverflow.com/a/47880734
// 注意：本工具固定使用 asm.js binding（zstd-codec-binding.js）。
// 原因：wasm binding 在 Node 进程退出时会触发 libuv 断言崩溃
// （Assertion failed: !(handle->flags & UV_HANDLE_CLOSING), src\win\async.c:94），
// 导致编辑命令 exit code 非零。asm.js 版本为纯 JS，无此问题，可干净退出。
const wasmSupported = false;

exports.run = (f) => {
    const Module = {};
    Module.onRuntimeInitialized = () => {
        f(Module);
    };

    if (wasmSupported) {
        require('./zstd-codec-binding-wasm.js')(Module);
    }
    else {
        require('./zstd-codec-binding.js')(Module);
    }
};
