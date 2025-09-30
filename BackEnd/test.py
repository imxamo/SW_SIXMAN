import sqlite3
conn = sqlite3.connect('cam_server.db')
conn.execute("DELETE FROM uploads WHERE file_path LIKE '%20250923_180549.txt%'")
conn.commit()
print("삭제 완료!")
conn.close()
