package com.aimeetingtable.mobile;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.UUID;

public final class FileBridge {
    private FileBridge() {
    }

    public static String importUriToPrivateFile(Context context, String uriText, String targetDirectory) {
        try {
            Uri uri = Uri.parse(uriText);
            ContentResolver resolver = context.getContentResolver();
            String name = displayName(resolver, uri);
            if (name == null || name.trim().isEmpty()) {
                name = "attachment";
            }
            name = name.replaceAll("[\\\\/:*?\"<>|]", "_");

            File directory = new File(targetDirectory);
            if (!directory.exists() && !directory.mkdirs()) {
                return "";
            }
            File target = null;
            try (InputStream input = resolver.openInputStream(uri)) {
                if (input == null) {
                    return "";
                }
                target = new File(directory, UUID.randomUUID().toString() + "-" + name);
                try (FileOutputStream output = new FileOutputStream(target)) {
                    byte[] buffer = new byte[8192];
                    int read;
                    while ((read = input.read(buffer)) >= 0) {
                        output.write(buffer, 0, read);
                    }
                }
            } catch (Exception exception) {
                if (target != null) {
                    target.delete();
                }
                return "";
            }
            return target.getAbsolutePath();
        } catch (Exception exception) {
            return "";
        }
    }

    private static String displayName(ContentResolver resolver, Uri uri) {
        try (Cursor cursor = resolver.query(uri, null, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (index >= 0) {
                    return cursor.getString(index);
                }
            }
        } catch (Exception exception) {
            return null;
        }
        String path = uri.getPath();
        if (path == null) {
            return null;
        }
        int slash = path.lastIndexOf('/');
        return slash >= 0 ? path.substring(slash + 1) : path;
    }
}
