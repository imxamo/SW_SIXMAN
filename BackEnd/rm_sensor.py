import sqlite3

conn = sqlite3.connect('sensor_server.db')
conn.execute("DELETE FROM uploads")  # 모든 행 삭제
conn.commit()
print("전체 데이터 삭제 완료!")
conn.close()
