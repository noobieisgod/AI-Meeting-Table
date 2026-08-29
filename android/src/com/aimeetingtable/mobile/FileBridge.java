package com.aimeetingtable.mobile;

import android.content.ContentResolver;
import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.content.ActivityNotFoundException;
import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;
import android.webkit.MimeTypeMap;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;

public final class FileBridge {
    public static final int OPENED = 0;
    public static final int FILE_MISSING = 1;
    public static final int NO_COMPATIBLE_APPLICATION = 2;
    public static final int ACCESS_DENIED = 3;
    private static final long WATCHDOG_POLL_MILLIS = 200L;
    private static final ConcurrentHashMap<String, ActiveOperation> ACTIVE = new ConcurrentHashMap<>();
    private static final ConcurrentHashMap<String, Integer> PENDING_CANCELLATIONS = new ConcurrentHashMap<>();
    private static final ExecutorService IMPORT_EXECUTOR = Executors.newCachedThreadPool(new ImportThreadFactory());

    private FileBridge() {
    }

    public static ImportResult importUriToPrivateFile(Context context,
                                                       String operationId,
                                                       String uriText,
                                                       String targetDirectory) {
        if (context == null || isBlank(operationId) || isBlank(uriText) || isBlank(targetDirectory)) {
            return ImportResult.failure(operationId, BoundedStreamCopier.STATUS_PROVIDER_FAILURE);
        }

        Integer pendingStatus = PENDING_CANCELLATIONS.remove(operationId);
        if (pendingStatus != null) {
            return ImportResult.failure(operationId, pendingStatus);
        }

        ActiveOperation operation = new ActiveOperation(operationId);
        if (ACTIVE.putIfAbsent(operationId, operation) != null) {
            return ImportResult.failure(operationId, BoundedStreamCopier.STATUS_PROVIDER_FAILURE);
        }
        pendingStatus = PENDING_CANCELLATIONS.remove(operationId);
        if (pendingStatus != null) {
            operation.cancel(pendingStatus);
        }

        Future<ImportResult> future = IMPORT_EXECUTOR.submit(
            () -> performImport(context, operation, uriText, targetDirectory));
        try {
            while (true) {
                if (operation.isCancelled()) {
                    future.cancel(true);
                    return ImportResult.failure(operationId, operation.cancellationStatus());
                }
                try {
                    ImportResult result = future.get(WATCHDOG_POLL_MILLIS, TimeUnit.MILLISECONDS);
                    if (result.statusCode == BoundedStreamCopier.STATUS_SUCCESS
                        && !operation.releaseSuccessfulFile(result.finalPath)) {
                        deleteQuietly(result.finalPath);
                        return ImportResult.failure(operationId, operation.cancellationStatus());
                    }
                    return result;
                } catch (TimeoutException exception) {
                    if (System.nanoTime() - operation.lastProgressNanos()
                        >= BoundedStreamCopier.NO_PROGRESS_TIMEOUT_NANOS) {
                        operation.cancel(BoundedStreamCopier.STATUS_TIMEOUT);
                        future.cancel(true);
                        return ImportResult.failure(operationId, BoundedStreamCopier.STATUS_TIMEOUT);
                    }
                } catch (InterruptedException exception) {
                    Thread.currentThread().interrupt();
                    operation.cancel(BoundedStreamCopier.STATUS_CANCELLED);
                    future.cancel(true);
                    return ImportResult.failure(operationId, BoundedStreamCopier.STATUS_CANCELLED);
                } catch (ExecutionException exception) {
                    operation.cancel(BoundedStreamCopier.STATUS_PROVIDER_FAILURE);
                    return ImportResult.failure(operationId, BoundedStreamCopier.STATUS_PROVIDER_FAILURE);
                }
            }
        } finally {
            ACTIVE.remove(operationId, operation);
            PENDING_CANCELLATIONS.remove(operationId);
        }
    }

    public static void cancelImport(String operationId, int statusCode) {
        int boundedStatus = statusCode == BoundedStreamCopier.STATUS_TIMEOUT
            ? BoundedStreamCopier.STATUS_TIMEOUT
            : BoundedStreamCopier.STATUS_CANCELLED;
        ActiveOperation operation = ACTIVE.get(operationId);
        if (operation != null) {
            operation.cancel(boundedStatus);
        } else if (!isBlank(operationId)) {
            PENDING_CANCELLATIONS.put(operationId, boundedStatus);
        }
    }

    public static void clearCancellation(String operationId) {
        if (!isBlank(operationId)) {
            PENDING_CANCELLATIONS.remove(operationId);
        }
    }

    public static int openPrivateFile(Context context, String path, String displayName) {
        if (context == null || isBlank(path)) {
            return FILE_MISSING;
        }
        File file = new File(path);
        if (!file.isFile()) {
            return FILE_MISSING;
        }

        try {
            String mimeType = mimeType(displayName);
            Uri uri = AttachmentContentProvider.register(context, file, displayName, mimeType);
            Intent view = new Intent(Intent.ACTION_VIEW)
                .setDataAndType(uri, mimeType)
                .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            view.setClipData(ClipData.newRawUri("Synsemble attachment", uri));
            if (view.resolveActivity(context.getPackageManager()) == null) {
                return NO_COMPATIBLE_APPLICATION;
            }
            Intent chooser = Intent.createChooser(view, "Open attachment")
                .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            if (!(context instanceof android.app.Activity)) {
                chooser.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            }
            context.startActivity(chooser);
            return OPENED;
        } catch (ActivityNotFoundException exception) {
            return NO_COMPATIBLE_APPLICATION;
        } catch (SecurityException | IOException exception) {
            return ACCESS_DENIED;
        }
    }

    private static String mimeType(String displayName) {
        String extension = MimeTypeMap.getFileExtensionFromUrl(
            Uri.encode(isBlank(displayName) ? "attachment" : displayName));
        String type = MimeTypeMap.getSingleton().getMimeTypeFromExtension(
            extension == null ? "" : extension.toLowerCase());
        return isBlank(type) ? "application/octet-stream" : type;
    }

    private static ImportResult performImport(Context context,
                                               ActiveOperation operation,
                                               String uriText,
                                               String targetDirectory) {
        try {
            Uri uri = Uri.parse(uriText);
            ContentResolver resolver = context.getContentResolver();
            Metadata metadata = metadata(resolver, uri);
            if (metadata.reportedSize > BoundedStreamCopier.MAX_ATTACHMENT_BYTES) {
                return ImportResult.failure(operation.operationId, BoundedStreamCopier.STATUS_TOO_LARGE);
            }
            if (operation.isCancelled()) {
                return ImportResult.failure(operation.operationId, operation.cancellationStatus());
            }

            InputStream input = resolver.openInputStream(uri);
            if (input == null) {
                return ImportResult.failure(operation.operationId, BoundedStreamCopier.STATUS_PROVIDER_FAILURE);
            }

            File directory = new File(targetDirectory);
            String finalName = UUID.randomUUID().toString() + "-" + sanitizeName(metadata.displayName);
            BoundedStreamCopier.Result copied = BoundedStreamCopier.copy(
                input,
                metadata.reportedSize,
                directory,
                finalName,
                BoundedStreamCopier.productionLimits(),
                operation,
                BoundedStreamCopier.productionDependencies());
            if (copied.status != BoundedStreamCopier.STATUS_SUCCESS) {
                return ImportResult.failure(operation.operationId, copied.status);
            }
            return ImportResult.success(operation.operationId,
                                        copied.finalFile.getAbsolutePath(),
                                        copied.byteCount,
                                        copied.sha256);
        } catch (Exception exception) {
            return ImportResult.failure(operation.operationId,
                                        operation.isCancelled()
                                            ? operation.cancellationStatus()
                                            : BoundedStreamCopier.STATUS_PROVIDER_FAILURE);
        }
    }

    private static Metadata metadata(ContentResolver resolver, Uri uri) {
        String name = null;
        long size = -1L;
        String[] projection = {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE};
        try (Cursor cursor = resolver.query(uri, projection, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (nameIndex >= 0 && !cursor.isNull(nameIndex)) {
                    name = cursor.getString(nameIndex);
                }
                int sizeIndex = cursor.getColumnIndex(OpenableColumns.SIZE);
                if (sizeIndex >= 0 && !cursor.isNull(sizeIndex)) {
                    size = cursor.getLong(sizeIndex);
                }
            }
        } catch (Exception ignored) {
            size = -1L;
        }
        if (isBlank(name)) {
            String path = uri.getPath();
            if (path != null) {
                int slash = path.lastIndexOf('/');
                name = slash >= 0 ? path.substring(slash + 1) : path;
            }
        }
        return new Metadata(isBlank(name) ? "attachment" : name, size);
    }

    private static String sanitizeName(String name) {
        String sanitized = isBlank(name) ? "attachment" : name.trim();
        return sanitized.replaceAll("[\\\\/:*?\"<>|]", "_");
    }

    private static boolean isBlank(String value) {
        return value == null || value.trim().isEmpty();
    }

    private static void deleteQuietly(String path) {
        if (!isBlank(path)) {
            new File(path).delete();
        }
    }

    public static final class ImportResult {
        private final String operationId;
        private final int statusCode;
        private final String finalPath;
        private final long byteCount;
        private final String sha256;

        private ImportResult(String operationId,
                             int statusCode,
                             String finalPath,
                             long byteCount,
                             String sha256) {
            this.operationId = operationId == null ? "" : operationId;
            this.statusCode = statusCode;
            this.finalPath = finalPath == null ? "" : finalPath;
            this.byteCount = byteCount;
            this.sha256 = sha256 == null ? "" : sha256;
        }

        static ImportResult success(String operationId, String finalPath, long byteCount, String sha256) {
            return new ImportResult(operationId,
                                    BoundedStreamCopier.STATUS_SUCCESS,
                                    finalPath,
                                    byteCount,
                                    sha256);
        }

        static ImportResult failure(String operationId, int statusCode) {
            return new ImportResult(operationId, statusCode, "", 0L, "");
        }

        public String getOperationId() {
            return operationId;
        }

        public int getStatusCode() {
            return statusCode;
        }

        public String getFinalPath() {
            return finalPath;
        }

        public long getByteCount() {
            return byteCount;
        }

        public String getSha256() {
            return sha256;
        }
    }

    private static final class Metadata {
        final String displayName;
        final long reportedSize;

        Metadata(String displayName, long reportedSize) {
            this.displayName = displayName;
            this.reportedSize = reportedSize;
        }
    }

    private static final class ActiveOperation implements BoundedStreamCopier.Control {
        final String operationId;
        private final AtomicInteger cancellationStatus = new AtomicInteger(BoundedStreamCopier.STATUS_SUCCESS);
        private final AtomicLong lastProgress = new AtomicLong(System.nanoTime());
        private volatile InputStream input;
        private File partFile;
        private File finalFile;
        private boolean released;

        ActiveOperation(String operationId) {
            this.operationId = operationId;
        }

        @Override
        public boolean isCancelled() {
            return cancellationStatus.get() != BoundedStreamCopier.STATUS_SUCCESS;
        }

        @Override
        public int cancellationStatus() {
            int status = cancellationStatus.get();
            return status == BoundedStreamCopier.STATUS_SUCCESS
                ? BoundedStreamCopier.STATUS_CANCELLED
                : status;
        }

        @Override
        public void setInput(InputStream value) {
            input = value;
            if (isCancelled()) {
                closeInput();
            }
        }

        @Override
        public void clearInput() {
            input = null;
        }

        @Override
        public synchronized void trackFiles(File part, File destination) {
            partFile = part;
            finalFile = destination;
            if (isCancelled() && !released) {
                deleteTrackedFiles();
            }
        }

        @Override
        public void onProgress(long copiedBytes) {
            lastProgress.set(System.nanoTime());
        }

        long lastProgressNanos() {
            return lastProgress.get();
        }

        synchronized void cancel(int status) {
            cancellationStatus.compareAndSet(BoundedStreamCopier.STATUS_SUCCESS, status);
            closeInput();
            if (!released) {
                deleteTrackedFiles();
            }
        }

        synchronized boolean releaseSuccessfulFile(String path) {
            if (isCancelled()) {
                deleteQuietly(path);
                return false;
            }
            released = true;
            partFile = null;
            finalFile = null;
            return true;
        }

        private void closeInput() {
            InputStream activeInput = input;
            if (activeInput != null) {
                try {
                    activeInput.close();
                } catch (IOException ignored) {
                }
            }
        }

        private void deleteTrackedFiles() {
            if (partFile != null) {
                partFile.delete();
            }
            if (finalFile != null) {
                finalFile.delete();
            }
        }
    }

    private static final class ImportThreadFactory implements ThreadFactory {
        private final AtomicInteger sequence = new AtomicInteger();

        @Override
        public Thread newThread(Runnable runnable) {
            Thread thread = new Thread(runnable, "attachment-import-" + sequence.incrementAndGet());
            thread.setDaemon(true);
            return thread;
        }
    }
}
