/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* Copyright © WHATWG (Apple, Google, Mozilla, Microsoft). This work is licensed under a Creative
 * Commons Attribution 4.0 International License. To the extent portions of it are incorporated into
 * source code, such portions in the source code are licensed under the BSD 3-Clause License
 * instead.
 *
 * The origin of this IDL file is
 * https://streams.spec.whatwg.org/#idl-index
 * `[Throws]` attribute for each method was determined by manually checking if the method can throw
 * an exception according to the spec.
 * To avoid API breakage, `[Throws]` attributes are omitted from `ReadableStream` methods.
 */

[Exposed=*, Transferable]
interface ReadableStream {
  constructor(optional object underlyingSource, optional QueuingStrategy strategy = {});

  readonly attribute boolean locked;

  Promise<undefined> cancel(optional any reason);
  ReadableStreamReader getReader(optional ReadableStreamGetReaderOptions options = {});
  ReadableStream pipeThrough(ReadableWritablePair transform, optional StreamPipeOptions options = {});
  Promise<undefined> pipeTo(WritableStream destination, optional StreamPipeOptions options = {});
  sequence<ReadableStream> tee();

  async iterable<any>(optional ReadableStreamIteratorOptions options = {});
};

typedef (ReadableStreamDefaultReader or ReadableStreamBYOBReader) ReadableStreamReader;

enum ReadableStreamReaderMode { "byob" };

dictionary ReadableStreamGetReaderOptions {
  ReadableStreamReaderMode mode;
};

dictionary ReadableStreamIteratorOptions {
  boolean preventCancel = false;
};

dictionary ReadableWritablePair {
  required ReadableStream readable;
  required WritableStream writable;
};

dictionary StreamPipeOptions {
  boolean preventClose = false;
  boolean preventAbort = false;
  boolean preventCancel = false;
  AbortSignal signal;
};

dictionary UnderlyingSource {
  UnderlyingSourceStartCallback start;
  UnderlyingSourcePullCallback pull;
  UnderlyingSourceCancelCallback cancel;
  ReadableStreamType type;
  [EnforceRange] unsigned long long autoAllocateChunkSize;
};

typedef (ReadableStreamDefaultController or ReadableByteStreamController) ReadableStreamController;

callback UnderlyingSourceStartCallback = any (ReadableStreamController controller);
callback UnderlyingSourcePullCallback = Promise<undefined> (ReadableStreamController controller);
callback UnderlyingSourceCancelCallback = Promise<undefined> (optional any reason);

enum ReadableStreamType { "bytes" };

interface mixin ReadableStreamGenericReader {
  readonly attribute Promise<undefined> closed;

  Promise<undefined> cancel(optional any reason);
};

[Exposed=*]
interface ReadableStreamDefaultReader {
  [Throws] constructor(ReadableStream stream);

  Promise<ReadableStreamReadResult> read();
  undefined releaseLock();
};
ReadableStreamDefaultReader includes ReadableStreamGenericReader;

dictionary ReadableStreamReadResult {
  any value;
  boolean done;
};

[Exposed=*]
interface ReadableStreamBYOBReader {
  [Throws] constructor(ReadableStream stream);

  Promise<ReadableStreamReadResult> read(ArrayBufferView view);
  undefined releaseLock();
};
ReadableStreamBYOBReader includes ReadableStreamGenericReader;

[Exposed=*]
interface ReadableStreamDefaultController {
  readonly attribute unrestricted double? desiredSize;

  [Throws] undefined close();
  [Throws] undefined enqueue(optional any chunk);
  undefined error(optional any e);
};

[Exposed=*]
interface ReadableByteStreamController {
  readonly attribute ReadableStreamBYOBRequest? byobRequest;
  readonly attribute unrestricted double? desiredSize;

  [Throws] undefined close();
  [Throws] undefined enqueue(ArrayBufferView chunk);
  undefined error(optional any e);
};

[Exposed=*]
interface ReadableStreamBYOBRequest {
  readonly attribute ArrayBufferView? view;

  [Throws] undefined respond([EnforceRange] unsigned long long bytesWritten);
  [Throws] undefined respondWithNewView(ArrayBufferView view);
};

[Exposed=*, Transferable]
interface WritableStream {
  [Throws] constructor(optional object underlyingSink, optional QueuingStrategy strategy = {});

  readonly attribute boolean locked;

  Promise<undefined> abort(optional any reason);
  Promise<undefined> close();
  [Throws] WritableStreamDefaultWriter getWriter();
};

dictionary UnderlyingSink {
  UnderlyingSinkStartCallback start;
  UnderlyingSinkWriteCallback write;
  UnderlyingSinkCloseCallback close;
  UnderlyingSinkAbortCallback abort;
  any type;
};

callback UnderlyingSinkStartCallback = any (WritableStreamDefaultController controller);
callback UnderlyingSinkWriteCallback = Promise<undefined> (any chunk, WritableStreamDefaultController controller);
callback UnderlyingSinkCloseCallback = Promise<undefined> ();
callback UnderlyingSinkAbortCallback = Promise<undefined> (optional any reason);

[Exposed=*]
interface WritableStreamDefaultWriter {
  [Throws] constructor(WritableStream stream);

  readonly attribute Promise<undefined> closed;
  [Throws] readonly attribute unrestricted double? desiredSize;
  readonly attribute Promise<undefined> ready;

  Promise<undefined> abort(optional any reason);
  Promise<undefined> close();
  undefined releaseLock();
  Promise<undefined> write(optional any chunk);
};

[Exposed=*]
interface WritableStreamDefaultController {
  readonly attribute AbortSignal signal;
  undefined error(optional any e);
};

[Exposed=*, Transferable]
interface TransformStream {
  [Throws]
  constructor(optional object transformer,
              optional QueuingStrategy writableStrategy = {},
              optional QueuingStrategy readableStrategy = {});

  readonly attribute ReadableStream readable;
  readonly attribute WritableStream writable;
};

dictionary Transformer {
  TransformerStartCallback start;
  TransformerTransformCallback transform;
  TransformerFlushCallback flush;
  any readableType;
  any writableType;
};

callback TransformerStartCallback = any (TransformStreamDefaultController controller);
callback TransformerFlushCallback = Promise<undefined> (TransformStreamDefaultController controller);
callback TransformerTransformCallback = Promise<undefined> (any chunk, TransformStreamDefaultController controller);

[Exposed=*]
interface TransformStreamDefaultController {
  readonly attribute unrestricted double? desiredSize;

  [Throws] undefined enqueue(optional any chunk);
  undefined error(optional any reason);
  undefined terminate();
};

dictionary QueuingStrategy {
  unrestricted double highWaterMark;
  QueuingStrategySize size;
};

callback QueuingStrategySize = unrestricted double (any chunk);

dictionary QueuingStrategyInit {
  required unrestricted double highWaterMark;
};

[Exposed=*]
interface ByteLengthQueuingStrategy {
  constructor(QueuingStrategyInit init);

  readonly attribute unrestricted double highWaterMark;
  readonly attribute Function size;
};

[Exposed=*]
interface CountQueuingStrategy {
  constructor(QueuingStrategyInit init);

  readonly attribute unrestricted double highWaterMark;
  readonly attribute Function size;
};

interface mixin GenericTransformStream {
  readonly attribute ReadableStream readable;
  readonly attribute WritableStream writable;
};
