package com.aimeetingtable.mobile;

import java.io.File;
import java.io.FileOutputStream;
import java.io.FilterOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Comparator;
import java.util.concurrent.atomic.AtomicLong;
import java.util.stream.Stream;

public final class BoundedStreamCopierTest {
    private static int passed;

    private BoundedStreamCopierTest() {
    }

    public static void main(String[] arguments) throws Exception {
        run("known reported oversize", BoundedStreamCopierTest::knownReportedOversize);
        run("unknown stream exceeds limit", BoundedStreamCopierTest::unknownStreamExceedsLimit);
        run("exact production limit succeeds", BoundedStreamCopierTest::exactProductionLimitSucceeds);
        run("one byte over production limit fails", BoundedStreamCopierTest::oneByteOverProductionLimitFails);
        run("short reads", BoundedStreamCopierTest::shortReads);
        run("incremental sha256", BoundedStreamCopierTest::incrementalSha256);
        run("cancellation cleanup", BoundedStreamCopierTest::cancellationCleanup);
        run("no progress timeout cleanup", BoundedStreamCopierTest::noProgressTimeoutCleanup);
        run("provider failure cleanup", BoundedStreamCopierTest::providerFailureCleanup);
        run("destination failure cleanup", BoundedStreamCopierTest::destinationFailureCleanup);
        run("insufficient storage", BoundedStreamCopierTest::insufficientStorage);
        run("hash failure cleanup", BoundedStreamCopierTest::hashFailureCleanup);
        run("rename failure cleanup", BoundedStreamCopierTest::renameFailureCleanup);
        run("successful typed result", BoundedStreamCopierTest::successfulTypedResult);
        System.out.println("Attachment import Java tests passed: " + passed);
    }

    private static void knownReportedOversize(Path directory) {
        CountingInputStream input = new CountingInputStream(1L, 1);
        BoundedStreamCopier.Limits limits = new BoundedStreamCopier.Limits(100L, 0L, 1_000_000L, 16);
        BoundedStreamCopier.Result result = copy(input, 101L, directory, limits, new TestControl(), defaults());
        require(result.status == BoundedStreamCopier.STATUS_TOO_LARGE, "known oversize was accepted");
        require(input.readCalls == 0, "known oversize source was read");
        requireDirectoryEmpty(directory);
    }

    private static void unknownStreamExceedsLimit(Path directory) {
        BoundedStreamCopier.Limits limits = new BoundedStreamCopier.Limits(1024L, 0L, 1_000_000L, 128);
        BoundedStreamCopier.Result result = copy(new CountingInputStream(1025L, 71), -1L, directory,
                                                 limits, new TestControl(), defaults());
        require(result.status == BoundedStreamCopier.STATUS_TOO_LARGE, "unknown oversize stream was accepted");
        requireDirectoryEmpty(directory);
    }

    private static void exactProductionLimitSucceeds(Path directory) {
        long size = BoundedStreamCopier.MAX_ATTACHMENT_BYTES;
        BoundedStreamCopier.Result result = copy(new CountingInputStream(size, 8191), size, directory,
                                                 BoundedStreamCopier.productionLimits(),
                                                 new TestControl(), defaults());
        require(result.status == BoundedStreamCopier.STATUS_SUCCESS, "exact production limit failed");
        require(result.byteCount == size, "exact production byte count mismatch");
        require(result.finalFile.isFile(), "exact production result missing");
        require(result.finalFile.delete(), "exact production result cleanup failed");
    }

    private static void oneByteOverProductionLimitFails(Path directory) {
        long size = BoundedStreamCopier.MAX_ATTACHMENT_BYTES + 1L;
        BoundedStreamCopier.Result result = copy(new CountingInputStream(size, 16381), -1L, directory,
                                                 BoundedStreamCopier.productionLimits(),
                                                 new TestControl(), defaults());
        require(result.status == BoundedStreamCopier.STATUS_TOO_LARGE, "one byte over limit was accepted");
        requireDirectoryEmpty(directory);
    }

    private static void shortReads(Path directory) {
        byte[] data = "short reads remain ordered".getBytes(StandardCharsets.UTF_8);
        BoundedStreamCopier.Result result = copy(new ByteArrayChunkInputStream(data, 3), -1L, directory,
                                                 smallLimits(), new TestControl(), defaults());
        require(result.status == BoundedStreamCopier.STATUS_SUCCESS, "short reads failed");
        try {
            require(java.util.Arrays.equals(Files.readAllBytes(result.finalFile.toPath()), data),
                    "short read content changed");
        } catch (IOException exception) {
            throw new AssertionError(exception);
        }
        require(result.finalFile.delete(), "short read result cleanup failed");
    }

    private static void incrementalSha256(Path directory) throws Exception {
        byte[] data = "incremental hash input".getBytes(StandardCharsets.UTF_8);
        BoundedStreamCopier.Result result = copy(new ByteArrayChunkInputStream(data, 2), -1L, directory,
                                                 smallLimits(), new TestControl(), defaults());
        require(result.status == BoundedStreamCopier.STATUS_SUCCESS, "hash import failed");
        require(result.sha256.equals(toHex(MessageDigest.getInstance("SHA-256").digest(data))),
                "incremental digest mismatch");
        require(result.finalFile.delete(), "hash result cleanup failed");
    }

    private static void cancellationCleanup(Path directory) {
        TestControl control = new TestControl();
        control.cancelAfterFirstProgress = true;
        BoundedStreamCopier.Result result = copy(new CountingInputStream(512L, 64), -1L, directory,
                                                 smallLimits(), control, defaults());
        require(result.status == BoundedStreamCopier.STATUS_CANCELLED, "cancel did not stop copy");
        requireDirectoryEmpty(directory);
    }

    private static void noProgressTimeoutCleanup(Path directory) {
        AtomicLong time = new AtomicLong();
        BoundedStreamCopier.Dependencies dependencies = new BoundedStreamCopier.Dependencies(
            () -> time.addAndGet(10L),
            ignored -> Long.MAX_VALUE,
            FileOutputStream::new,
            () -> MessageDigest.getInstance("SHA-256"),
            BoundedStreamCopierTest::move);
        BoundedStreamCopier.Limits limits = new BoundedStreamCopier.Limits(100L, 0L, 25L, 16);
        BoundedStreamCopier.Result result = copy(new ZeroReadInputStream(), -1L, directory,
                                                 limits, new TestControl(), dependencies);
        require(result.status == BoundedStreamCopier.STATUS_TIMEOUT, "no progress did not time out");
        requireDirectoryEmpty(directory);
    }

    private static void providerFailureCleanup(Path directory) {
        BoundedStreamCopier.Result result = copy(new FailingInputStream(), -1L, directory,
                                                 smallLimits(), new TestControl(), defaults());
        require(result.status == BoundedStreamCopier.STATUS_PROVIDER_FAILURE, "provider failure misclassified");
        requireDirectoryEmpty(directory);
    }

    private static void destinationFailureCleanup(Path directory) {
        BoundedStreamCopier.OutputFactory failingOutput = destination -> {
            FileOutputStream file = new FileOutputStream(destination);
            return new FilterOutputStream(file) {
                @Override
                public void write(byte[] bytes, int offset, int length) throws IOException {
                    out.write(bytes, offset, Math.min(1, length));
                    throw new IOException("synthetic destination failure");
                }
            };
        };
        BoundedStreamCopier.Dependencies dependencies = dependencies(failingOutput,
                                                                     () -> MessageDigest.getInstance("SHA-256"),
                                                                     BoundedStreamCopierTest::move,
                                                                     ignored -> Long.MAX_VALUE);
        BoundedStreamCopier.Result result = copy(new CountingInputStream(64L, 64), -1L, directory,
                                                 smallLimits(), new TestControl(), dependencies);
        require(result.status == BoundedStreamCopier.STATUS_DESTINATION_FAILURE,
                "destination failure misclassified");
        requireDirectoryEmpty(directory);
    }

    private static void insufficientStorage(Path directory) {
        BoundedStreamCopier.Limits limits = new BoundedStreamCopier.Limits(100L, 64L, 1_000_000L, 16);
        BoundedStreamCopier.Dependencies dependencies = dependencies(FileOutputStream::new,
                                                                     () -> MessageDigest.getInstance("SHA-256"),
                                                                     BoundedStreamCopierTest::move,
                                                                     ignored -> 70L);
        BoundedStreamCopier.Result result = copy(new CountingInputStream(10L, 10), 10L, directory,
                                                 limits, new TestControl(), dependencies);
        require(result.status == BoundedStreamCopier.STATUS_INSUFFICIENT_STORAGE,
                "insufficient storage was accepted");
        requireDirectoryEmpty(directory);
    }

    private static void hashFailureCleanup(Path directory) {
        BoundedStreamCopier.Dependencies dependencies = dependencies(FileOutputStream::new,
                                                                     () -> { throw new NoSuchAlgorithmException("synthetic"); },
                                                                     BoundedStreamCopierTest::move,
                                                                     ignored -> Long.MAX_VALUE);
        BoundedStreamCopier.Result result = copy(new CountingInputStream(10L, 10), -1L, directory,
                                                 smallLimits(), new TestControl(), dependencies);
        require(result.status == BoundedStreamCopier.STATUS_HASH_FAILURE, "hash failure misclassified");
        requireDirectoryEmpty(directory);
    }

    private static void renameFailureCleanup(Path directory) {
        BoundedStreamCopier.Dependencies dependencies = dependencies(FileOutputStream::new,
                                                                     () -> MessageDigest.getInstance("SHA-256"),
                                                                     (source, target) -> false,
                                                                     ignored -> Long.MAX_VALUE);
        BoundedStreamCopier.Result result = copy(new CountingInputStream(10L, 10), -1L, directory,
                                                 smallLimits(), new TestControl(), dependencies);
        require(result.status == BoundedStreamCopier.STATUS_RENAME_FAILURE, "rename failure misclassified");
        requireDirectoryEmpty(directory);
    }

    private static void successfulTypedResult(Path directory) {
        BoundedStreamCopier.Result result = copy(new CountingInputStream(37L, 5), -1L, directory,
                                                 smallLimits(), new TestControl(), defaults());
        require(result.status == BoundedStreamCopier.STATUS_SUCCESS, "valid import failed");
        require(result.finalFile.isFile(), "success path missing");
        require(result.byteCount == 37L, "success byte count mismatch");
        require(result.sha256.matches("[0-9a-f]{64}"), "success digest invalid");
        require(!result.finalFile.getName().endsWith(".part"), "partial name was returned");
        require(result.finalFile.delete(), "success result cleanup failed");
    }

    private static BoundedStreamCopier.Result copy(InputStream input,
                                                   long reportedSize,
                                                   Path directory,
                                                   BoundedStreamCopier.Limits limits,
                                                   TestControl control,
                                                   BoundedStreamCopier.Dependencies dependencies) {
        return BoundedStreamCopier.copy(input, reportedSize, directory.toFile(), "result.bin",
                                        limits, control, dependencies);
    }

    private static BoundedStreamCopier.Limits smallLimits() {
        return new BoundedStreamCopier.Limits(4096L, 0L, 1_000_000L, 64);
    }

    private static BoundedStreamCopier.Dependencies defaults() {
        return dependencies(FileOutputStream::new,
                            () -> MessageDigest.getInstance("SHA-256"),
                            BoundedStreamCopierTest::move,
                            ignored -> Long.MAX_VALUE);
    }

    private static BoundedStreamCopier.Dependencies dependencies(
        BoundedStreamCopier.OutputFactory outputFactory,
        BoundedStreamCopier.DigestFactory digestFactory,
        BoundedStreamCopier.Renamer renamer,
        BoundedStreamCopier.SpaceChecker spaceChecker) {
        return new BoundedStreamCopier.Dependencies(System::nanoTime,
                                                    spaceChecker,
                                                    outputFactory,
                                                    digestFactory,
                                                    renamer);
    }

    private static boolean move(File source, File target) throws IOException {
        Files.move(source.toPath(), target.toPath());
        return target.isFile() && !source.exists();
    }

    private static String toHex(byte[] bytes) {
        StringBuilder result = new StringBuilder(bytes.length * 2);
        for (byte value : bytes) {
            result.append(String.format("%02x", value & 0xff));
        }
        return result.toString();
    }

    private static void run(String name, TestCase test) throws Exception {
        Path directory = Files.createTempDirectory("attachment-import-test-");
        try {
            test.run(directory);
            passed += 1;
            System.out.println("PASS: " + name);
        } finally {
            deleteTree(directory);
        }
    }

    private static void requireDirectoryEmpty(Path directory) {
        try (Stream<Path> entries = Files.list(directory)) {
            require(entries.findAny().isEmpty(), "temporary output was not removed");
        } catch (IOException exception) {
            throw new AssertionError(exception);
        }
    }

    private static void deleteTree(Path root) throws IOException {
        if (!Files.exists(root)) {
            return;
        }
        try (Stream<Path> paths = Files.walk(root)) {
            for (Path path : paths.sorted(Comparator.reverseOrder()).toList()) {
                Files.deleteIfExists(path);
            }
        }
    }

    private static void require(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError(message);
        }
    }

    private interface TestCase {
        void run(Path directory) throws Exception;
    }

    private static final class TestControl implements BoundedStreamCopier.Control {
        boolean cancelled;
        boolean cancelAfterFirstProgress;

        @Override
        public boolean isCancelled() {
            return cancelled;
        }

        @Override
        public int cancellationStatus() {
            return BoundedStreamCopier.STATUS_CANCELLED;
        }

        @Override
        public void setInput(InputStream input) {
        }

        @Override
        public void clearInput() {
        }

        @Override
        public void trackFiles(File partFile, File finalFile) {
        }

        @Override
        public void onProgress(long copiedBytes) {
            if (cancelAfterFirstProgress) {
                cancelled = true;
            }
        }
    }

    private static final class CountingInputStream extends InputStream {
        private long remaining;
        private final int maximumChunk;
        int readCalls;

        CountingInputStream(long remaining, int maximumChunk) {
            this.remaining = remaining;
            this.maximumChunk = maximumChunk;
        }

        @Override
        public int read() {
            byte[] single = new byte[1];
            try {
                return read(single, 0, 1) < 0 ? -1 : single[0] & 0xff;
            } catch (IOException exception) {
                throw new AssertionError(exception);
            }
        }

        @Override
        public int read(byte[] bytes, int offset, int length) throws IOException {
            readCalls += 1;
            if (remaining == 0L) {
                return -1;
            }
            int count = (int)Math.min(remaining, Math.min(length, maximumChunk));
            for (int index = 0; index < count; index += 1) {
                bytes[offset + index] = (byte)((remaining + index) % 251L);
            }
            remaining -= count;
            return count;
        }
    }

    private static final class ByteArrayChunkInputStream extends InputStream {
        private final byte[] data;
        private final int maximumChunk;
        private int position;

        ByteArrayChunkInputStream(byte[] data, int maximumChunk) {
            this.data = data.clone();
            this.maximumChunk = maximumChunk;
        }

        @Override
        public int read() {
            return position >= data.length ? -1 : data[position++] & 0xff;
        }

        @Override
        public int read(byte[] bytes, int offset, int length) {
            if (position >= data.length) {
                return -1;
            }
            int count = Math.min(data.length - position, Math.min(length, maximumChunk));
            System.arraycopy(data, position, bytes, offset, count);
            position += count;
            return count;
        }
    }

    private static final class ZeroReadInputStream extends InputStream {
        @Override
        public int read() {
            return 0;
        }

        @Override
        public int read(byte[] bytes, int offset, int length) {
            return 0;
        }
    }

    private static final class FailingInputStream extends InputStream {
        private boolean providedData;

        @Override
        public int read() throws IOException {
            throw new IOException("synthetic provider failure");
        }

        @Override
        public int read(byte[] bytes, int offset, int length) throws IOException {
            if (!providedData) {
                providedData = true;
                bytes[offset] = 1;
                return 1;
            }
            throw new IOException("synthetic provider failure");
        }
    }
}
