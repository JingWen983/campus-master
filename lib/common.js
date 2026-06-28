/**
 * common.js - 公共工具函数库
 * 提供 API 请求、登录校验、登出、日期格式化等通用能力，
 * 供 index.html / admin.html / teacher.html / student.html 共享使用。
 */

// API 基础地址（同源部署时为空字符串）
const API_BASE = '';

/**
 * 统一的 API 请求封装
 * @param {string} method - HTTP 方法：GET / POST / PUT / DELETE
 * @param {string} url - 请求地址（相对路径，如 '/api/admin/users'）
 * @param {object|null} [data] - 请求体数据（仅 POST/PUT/DELETE 带体时传入）
 * @returns {Promise<any>} 解析后的 JSON 响应
 */
async function apiRequest(method, url, data) {
    const options = {
        method: method,
        credentials: 'include',
        headers: {}
    };

    if (data !== undefined && data !== null) {
        options.headers['Content-Type'] = 'application/json';
        options.body = JSON.stringify(data);
    } else if (method === 'DELETE') {
        // 无请求体的 DELETE 请求需要 Content-Length: 0 头，
        // 否则部分后端 / 反向代理会拒绝该请求。
        options.headers['Content-Length'] = '0';
    }

    const response = await fetch(API_BASE + url, options);
    return await response.json();
}

/**
 * 校验登录状态与角色权限
 * @param {number} roleId - 期望的角色 ID：1=管理员 / 2=教师 / 3=学生
 * @returns {object|null} 校验通过返回 userInfo，否则跳转登录页并返回 null
 */
function checkAuth(roleId) {
    const userInfoStr = localStorage.getItem('userInfo');

    if (!userInfoStr) {
        window.location.href = '/';
        return null;
    }

    try {
        const userInfo = JSON.parse(userInfoStr);
        if (userInfo.role_id !== roleId) {
            const roleNames = { 1: '管理员', 2: '教师', 3: '学生', 4: '家长' };
            alert(`您不是${roleNames[roleId]}，无法访问此页面`);
            window.location.href = '/';
            return null;
        }
        return userInfo;
    } catch (e) {
        window.location.href = '/';
        return null;
    }
}

/**
 * 登出：清除本地存储并跳转到登录页
 */
async function logout() {
    try {
        await apiRequest('POST', '/api/auth/logout');
    } catch (e) {
        // 忽略错误，继续跳转
    }
    localStorage.removeItem('userInfo');
    window.location.href = '/';
}

/**
 * 格式化日期时间字符串
 * @param {string|number|Date} dateStr - 可被 Date 解析的日期时间
 * @returns {string} 格式化后的 'YYYY-MM-DD HH:mm:ss' 字符串；解析失败时返回原值
 */
function formatDateTime(dateStr) {
    if (!dateStr) return '';
    const date = new Date(dateStr);
    if (isNaN(date.getTime())) return dateStr;

    const pad = (n) => String(n).padStart(2, '0');
    const year = date.getFullYear();
    const month = pad(date.getMonth() + 1);
    const day = pad(date.getDate());
    const hours = pad(date.getHours());
    const minutes = pad(date.getMinutes());
    const seconds = pad(date.getSeconds());

    return `${year}-${month}-${day} ${hours}:${minutes}:${seconds}`;
}
