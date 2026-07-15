package com.aimeetingtable.mobile;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;

public final class AttachmentContentProvider extends ContentProvider {
    private static final String AUTHORITY = "com.aimeetingtable.myapp.attachments";
    private static final Map<String, SharedFile> FILES = new ConcurrentHashMap<>();

    static Uri register(Context context, File source, String displayName, String mimeType)
        throws IOException {
        File file = source.getCanonicalFile();
        File dataDirectory = context.getDataDir().getCanonicalFile();
        String dataPrefix = dataDirectory.getPath() + File.separator;
        if (!file.isFile() || !file.getPath().startsWith(dataPrefix)) {
            throw new SecurityException("Attachment is outside private application storage");
        }
        String token = UUID.randomUUID().toString();
        FILES.put(token, new SharedFile(file,
                                       isBlank(displayName) ? file.getName() : displayName,
                                       isBlank(mimeType) ? "application/octet-stream" : mimeType));
        return new Uri.Builder().scheme("content").authority(AUTHORITY).appendPath(token).build();
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public String getType(Uri uri) {
        SharedFile shared = sharedFile(uri);
        return shared == null ? null : shared.mimeType;
    }

    @Override
    public Cursor query(Uri uri,
                        String[] projection,
                        String selection,
                        String[] selectionArgs,
                        String sortOrder) {
        SharedFile shared = sharedFile(uri);
        if (shared == null) {
            return null;
        }
        String[] columns = projection == null
            ? new String[] {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE}
            : projection;
        MatrixCursor cursor = new MatrixCursor(columns, 1);
        MatrixCursor.RowBuilder row = cursor.newRow();
        for (String column : columns) {
            if (OpenableColumns.DISPLAY_NAME.equals(column)) {
                row.add(shared.displayName);
            } else if (OpenableColumns.SIZE.equals(column)) {
                row.add(shared.file.length());
            } else {
                row.add(null);
            }
        }
        return cursor;
    }

    @Override
    public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        if (!"r".equals(mode)) {
            throw new SecurityException("Attachments are read-only");
        }
        SharedFile shared = sharedFile(uri);
        if (shared == null || !shared.file.isFile()) {
            throw new FileNotFoundException("Attachment is unavailable");
        }
        return ParcelFileDescriptor.open(shared.file, ParcelFileDescriptor.MODE_READ_ONLY);
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        throw new UnsupportedOperationException("Read-only provider");
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException("Read-only provider");
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException("Read-only provider");
    }

    private static SharedFile sharedFile(Uri uri) {
        if (uri == null || !AUTHORITY.equals(uri.getAuthority())) {
            return null;
        }
        String token = uri.getLastPathSegment();
        return token == null ? null : FILES.get(token);
    }

    private static boolean isBlank(String value) {
        return value == null || value.trim().isEmpty();
    }

    private static final class SharedFile {
        final File file;
        final String displayName;
        final String mimeType;

        SharedFile(File file, String displayName, String mimeType) {
            this.file = file;
            this.displayName = displayName;
            this.mimeType = mimeType;
        }
    }
}
