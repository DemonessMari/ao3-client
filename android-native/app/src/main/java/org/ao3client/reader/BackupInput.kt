package org.ao3client.reader
fun readBackupBytes(stream:java.io.InputStream):ByteArray {
    val output=java.io.ByteArrayOutputStream(); val buffer=ByteArray(8192)
    while(true) { val count=stream.read(buffer); if(count<0) break; require(output.size()+count<=LibraryBackup.MAX_BYTES) { "Backup exceeds 100 MB" }; output.write(buffer,0,count) }
    return output.toByteArray()
}
