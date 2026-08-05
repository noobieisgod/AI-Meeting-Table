package com.aimeetingtable.mobile;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.AtomicMoveNotSupportedException;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

final class BoundedStreamCopier {
    static final long MAX_ATTACHMENT_BYTES = 25L * 1024L * 1024L;
    static final long FREE_SPACE_RESERVE_BYTES = 64L * 1024L * 1024L;
    static final long NO_PROGRESS_TIMEOUT_NANOS = 60L * 1_000_000_000L;
    static final int BUFFER_BYTES = 64 * 1024;

    static final int STATUS_SUCCESS = 0;
    static final int STATUS_TOO_LARGE = 1;
    static final int STATUS_INSUFFICIENT_STORAGE = 2;
    static final int STATUS_PROVIDER_FAILURE = 3;
    static final int STATUS_DESTINATION_FAILURE = 4;
    static final int STATUS_CANCELLED = 5;
    static final int STATUS_TIMEOUT = 6;
    static final int STATUS_HASH_FAILURE = 7;
    static final int STATUS_RENAME_FAILURE = 8;

    private BoundedStreamCopier() {
    }

    static Limits productionLimits() {
        return new Limits(MAX_ATTACHMENT_BYTES,
                          FREE_SPACE_RESERVE_BYTES,
                          NO_PROGRESS_TIMEOUT_NANOS,
                          BUFFER_BYTES);
    }

    static Dependencies productionDependencies() {
        return new Dependencies(System::nanoTime,
                                File::getUsableSpace,
                                FileOutputStream::new,
                                () -> MessageDigest.getInstance("SHA-256"),
                                BoundedStreamCopier::moveSafely);
    }

    static Result copy(InputStream input,
                       long reportedSize,
                       File directory,
                       String finalName,
                       Limits limits,
                       Control control,
                       Dependencies dependencies) {
        if (input == null) {
            return Result.failure(STATUS_PROVIDER_FAILURE);
        }
        if (reportedSize > limits.maximumBytes) {
            closeQuietly(input);
            return Result.failure(STATUS_TOO_LARGE);
        }
        if ((!directory.exists() && !directory.mkdirs()) || !directory.isDirectory()) {
            closeQuietly(input);
            return Result.failure(STATUS_DESTINATION_FAILURE);
        }

        File finalFile = new File(directory, finalName);
        File partFile = new File(directory, finalName + ".part");
        control.trackFiles(partFile, finalFile);
        deleteQuietly(partFile);

        long initialAvailable = dependencies.spaceChecker.usableSpace(directory);
        long earlyRequired = reportedSize >= 0L ? reportedSize : 0L;
        if (!hasSpace(initialAvailable, earlyRequired, limits.reserveBytes)) {
            closeQuietly(input);
            control.clearInput();
            deleteQuietly(partFile);
            return Result.failure(STATUS_INSUFFICIENT_STORAGE);
        }

        OutputStream output = null;
        boolean success = false;
        boolean finalCreated = false;
        try {
            final MessageDigest digest;
            try {
                digest = dependencies.digestFactory.create();
            } catch (NoSuchAlgorithmException exception) {
                return Result.failure(STATUS_HASH_FAILURE);
            }

            control.setInput(input);
            try {
                output = dependencies.outputFactory.open(partFile);
            } catch (IOException exception) {
                return Result.failure(STATUS_DESTINATION_FAILURE);
            }

            byte[] buffer = new byte[limits.bufferBytes];
            long copied = 0L;
            long lastProgress = dependencies.clock.nanoTime();
            while (true) {
                if (control.isCancelled()) {
                    return Result.failure(control.cancellationStatus());
                }

                final int read;
                try {
                    read = input.read(buffer);
                } catch (IOException exception) {
                    return Result.failure(control.isCancelled()
                                              ? control.cancellationStatus()
                                              : STATUS_PROVIDER_FAILURE);
                }

                if (read < 0) {
                    break;
                }
                if (read == 0) {
                    if (dependencies.clock.nanoTime() - lastProgress >= limits.noProgressTimeoutNanos) {
                        return Result.failure(STATUS_TIMEOUT);
                    }
                    Thread.yield();
                    continue;
                }
                if (control.isCancelled()) {
                    return Result.failure(control.cancellationStatus());
                }
                if (copied > limits.maximumBytes - read) {
                    return Result.failure(STATUS_TOO_LARGE);
                }

                long currentAvailable = dependencies.spaceChecker.usableSpace(directory);
                long accountedAvailable = initialAvailable == Long.MAX_VALUE
                    ? Long.MAX_VALUE
                    : Math.max(0L, initialAvailable - copied);
                long effectiveAvailable = Math.min(currentAvailable, accountedAvailable);
                if (!hasSpace(effectiveAvailable, read, limits.reserveBytes)) {
                    return Result.failure(STATUS_INSUFFICIENT_STORAGE);
                }

                try {
                    output.write(buffer, 0, read);
                } catch (IOException exception) {
                    return Result.failure(STATUS_DESTINATION_FAILURE);
                }
                digest.update(buffer, 0, read);
                copied += read;
                lastProgress = dependencies.clock.nanoTime();
                control.onProgress(copied);
            }

            try {
                output.flush();
                output.close();
                output = null;
            } catch (IOException exception) {
                return Result.failure(STATUS_DESTINATION_FAILURE);
            }
            closeQuietly(input);
            input = null;
            control.clearInput();

            if (control.isCancelled()) {
                return Result.failure(control.cancellationStatus());
            }
            try {
                if (!dependencies.renamer.move(partFile, finalFile)) {
                    return Result.failure(STATUS_RENAME_FAILURE);
                }
                finalCreated = true;
            } catch (IOException exception) {
                return Result.failure(STATUS_RENAME_FAILURE);
            }
            if (control.isCancelled()) {
                return Result.failure(control.cancellationStatus());
            }

            success = true;
            return Result.success(finalFile, copied, toHex(digest.digest()));
        } finally {
            if (output != null) {
                closeQuietly(output);
            }
            if (input != null) {
                closeQuietly(input);
            }
            control.clearInput();
            if (!success) {
                deleteQuietly(partFile);
                if (finalCreated || finalFile.exists()) {
                    deleteQuietly(finalFile);
                }
            }
        }
    }

    private static boolean hasSpace(long available, long required, long reserve) {
        return available >= reserve && required <= available - reserve;
    }

    private static boolean moveSafely(File source, File destination) throws IOException {
        try {
            Files.move(source.toPath(), destination.toPath(), StandardCopyOption.ATOMIC_MOVE);
        } catch (AtomicMoveNotSupportedException exception) {
            Files.move(source.toPath(), destination.toPath());
        }
        return destination.isFile() && !source.exists();
    }

    private static String toHex(byte[] bytes) {
        StringBuilder result = new StringBuilder(bytes.length * 2);
        for (byte value : bytes) {
            result.append(String.format("%02x", value & 0xff));
        }
        return result.toString();
    }

    private static void closeQuietly(InputStream input) {
        try {
            input.close();
        } catch (IOException ignored) {
        }
    }

    private static void closeQuietly(OutputStream output) {
        try {
            output.close();
        } catch (IOException ignored) {
        }
    }

    private static void deleteQuietly(File file) {
        try {
            Files.deleteIfExists(file.toPath());
        } catch (IOException ignored) {
        }
    }

    static final class Limits {
        final long maximumBytes;
        final long reserveBytes;
        final long noProgressTimeoutNanos;
        final int bufferBytes;

        Limits(long maximumBytes, long reserveBytes, long noProgressTimeoutNanos, int bufferBytes) {
            if (maximumBytes < 0L || reserveBytes < 0L || noProgressTimeoutNanos <= 0L || bufferBytes <= 0) {
                throw new IllegalArgumentException("Invalid attachment import limits");
            }
            this.maximumBytes = maximumBytes;
            this.reserveBytes = reserveBytes;
            this.noProgressTimeoutNanos = noProgressTimeoutNanos;
            this.bufferBytes = bufferBytes;
        }
    }

    interface Control {
        boolean isCancelled();
        int cancellationStatus();
        void setInput(InputStream input);
        void clearInput();
        void trackFiles(File partFile, File finalFile);
        void onProgress(long copiedBytes);
    }

    interface Clock {
        long nanoTime();
    }

    interface SpaceChecker {
        long usableSpace(File directory);
    }

    interface OutputFactory {
        OutputStream open(File destination) throws IOException;
    }

    interface DigestFactory {
        MessageDigest create() throws NoSuchAlgorithmException;
    }

    interface Renamer {
        boolean move(File source, File destination) throws IOException;
    }

    static final class Dependencies {
        final Clock clock;
        final SpaceChecker spaceChecker;
        final OutputFactory outputFactory;
        final DigestFactory digestFactory;
        final Renamer renamer;

        Dependencies(Clock clock,
                     SpaceChecker spaceChecker,
                     OutputFactory outputFactory,
                     DigestFactory digestFactory,
                     Renamer renamer) {
            this.clock = clock;
            this.spaceChecker = spaceChecker;
            this.outputFactory = outputFactory;
            this.digestFactory = digestFactory;
            this.renamer = renamer;
        }
    }

    static final class Result {
        final int status;
        final File finalFile;
        final long byteCount;
        final String sha256;

        private Result(int status, File finalFile, long byteCount, String sha256) {
            this.status = status;
            this.finalFile = finalFile;
            this.byteCount = byteCount;
            this.sha256 = sha256;
        }

        static Result success(File finalFile, long byteCount, String sha256) {
            return new Result(STATUS_SUCCESS, finalFile, byteCount, sha256);
        }

        static Result failure(int status) {
            return new Result(status, null, 0L, "");
        }
    }
}
