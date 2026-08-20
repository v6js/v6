public final class V6WebGlobals {
  static V6Value fn(V6Callable c) {
    return V6Builtins.fn(c);
  }

  static V6Value objValue(V6Object o) {
    return V6Builtins.objValue(o);
  }

  public static final V6Value EVENT_CTOR = objValue(new V6EventConstructor());
  public static final V6Value CUSTOM_EVENT_CTOR =
      objValue(new V6CustomEventConstructor());
  public static final V6Value EVENT_TARGET_CTOR =
      objValue(new V6EventTargetConstructor());
  public static final V6Value ABORT_SIGNAL_CTOR =
      objValue(new V6AbortSignalConstructor());
  public static final V6Value ABORT_CONTROLLER_CTOR =
      objValue(new V6AbortControllerConstructor());
  public static final V6Value STRUCTURED_CLONE =
      fn((thisArg, args) -> V6StructuredClone.clone(V6Value.argAt(args, 0)));
  public static final V6Value TEXT_ENCODER_CTOR =
      objValue(new V6TextEncoderConstructor());
  public static final V6Value TEXT_DECODER_CTOR =
      objValue(new V6TextDecoderConstructor());
  public static final V6Value READABLE_STREAM_CTOR =
      objValue(new V6ReadableStreamConstructor());
  public static final V6Value WRITABLE_STREAM_CTOR =
      objValue(new V6WritableStreamConstructor());
  public static final V6Value TRANSFORM_STREAM_CTOR =
      objValue(new V6TransformStreamConstructor());
  public static final V6Value COUNT_QUEUING_STRATEGY_CTOR =
      objValue(new V6CountQueuingStrategyConstructor());
  public static final V6Value BYTE_LENGTH_QUEUING_STRATEGY_CTOR =
      objValue(new V6ByteLengthQueuingStrategyConstructor());
  public static final V6Value ARRAY_BUFFER_CTOR =
      objValue(new V6ArrayBufferConstructor());
  public static final V6Value BLOB_CTOR = objValue(new V6BlobConstructor());
  public static final V6Value FILE_CTOR = objValue(new V6FileConstructor());
  public static final V6Value FORM_DATA_CTOR =
      objValue(new V6FormDataConstructor());
  public static final V6Value TEXT_ENCODER_STREAM_CTOR =
      objValue(new V6TextEncoderStreamConstructor());
  public static final V6Value TEXT_DECODER_STREAM_CTOR =
      objValue(new V6TextDecoderStreamConstructor());
  public static final V6Value COMPRESSION_STREAM_CTOR =
      objValue(new V6CompressionStreamConstructor());
  public static final V6Value DECOMPRESSION_STREAM_CTOR =
      objValue(new V6DecompressionStreamConstructor());
  public static final V6Value HEADERS_CTOR =
      objValue(new V6HeadersConstructor());
  public static final V6Value REQUEST_CTOR =
      objValue(new V6RequestConstructor());
  public static final V6Value RESPONSE_CTOR =
      objValue(new V6ResponseConstructor());
  public static final V6Value FETCH =
      fn((thisArg, args) -> V6Fetch.fetch(args));
  public static final V6Value WEBSOCKET_CTOR =
      objValue(new V6WebSocketConstructor());
  public static final V6Value EVENT_SOURCE_CTOR =
      objValue(new V6EventSourceConstructor());
  public static final V6Value CRYPTO_KEY_CTOR =
      objValue(new V6CryptoKeyConstructor());
  public static final V6Value WEB_CRYPTO = objValue(V6WebCrypto.build());
  public static final V6Value MESSAGE_EVENT_CTOR =
      objValue(new V6MessageEventConstructor());
  public static final V6Value MESSAGE_PORT_CTOR =
      objValue(new V6MessagePortConstructor());
  public static final V6Value MESSAGE_CHANNEL_CTOR =
      objValue(new V6WebMessageChannelConstructor());
  public static final V6Value BROADCAST_CHANNEL_CTOR =
      objValue(new V6BroadcastChannelConstructor());
  public static final V6Value WEB_WORKER_CTOR =
      objValue(new V6WebWorkerConstructor());
  public static final V6Value PERFORMANCE = V6PerfHooks.performance();
  public static final V6Value NAVIGATOR = objValue(V6Navigator.build());
  public static final V6Value WORKER_SELF = V6WorkerThreads.selfScope();
  public static final V6Value WEBASSEMBLY = objValue(V6WasmGlobal.build());
}
