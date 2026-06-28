"""验证兑换后端积分校验"""
import requests
BASE = "http://localhost:8080"

s = requests.Session()
r = s.post(f"{BASE}/api/auth/login", json={"username": "student", "password": "student123"})
print("login:", r.json().get("code"))

# 查询商城商品真实价格
r = s.get(f"{BASE}/api/student/mall")
items = r.json().get("data", [])
print("商品价格:", [(i.get("id"), i.get("name"), i.get("price")) for i in items[:3]])

if not items:
    print("无商品，测试终止")
    exit()

target = items[0]
target_id = target["id"]
target_name = target["name"]
real_price = target["price"]
print(f"目标: {target_name} 真实价 {real_price}")

# 攻击测试1: 传入伪造低价 cost=1
print(f"攻击1: 伪造 cost=1 (真实价 {real_price})")
r = s.post(f"{BASE}/api/mall/redeem", json={"item_id": target_id, "cost": 1})
print("  结果:", r.json().get("code"), r.json().get("msg"))

# 攻击测试2: 不传 cost
print("攻击2: 不传 cost")
r = s.post(f"{BASE}/api/mall/redeem", json={"item_id": target_id})
print("  结果:", r.json().get("code"), r.json().get("msg"))

# 正常兑换
print(f"正常兑换: 传 cost={real_price}")
r = s.post(f"{BASE}/api/mall/redeem", json={"item_id": target_id, "cost": real_price})
print("  结果:", r.json().get("code"), r.json().get("msg"), "剩余:", r.json().get("data", {}).get("remain_points"))
