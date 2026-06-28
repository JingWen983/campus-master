import sqlite3
conn = sqlite3.connect("d:/邵敬文/comptation/campus_system.db")
c = conn.cursor()
c.execute("SELECT id, username, name, points FROM users WHERE role_id=3")
for row in c.fetchall():
    print(row)
print("---redemptions---")
c.execute("SELECT id, student_id, item_id, cost, created_at FROM redemption_records ORDER BY id DESC LIMIT 5")
for row in c.fetchall():
    print(row)
conn.close()
