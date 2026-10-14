const API_BASE = "http://127.0.0.1:8000";

export async function request(path, options = {}) {
  const token = localStorage.getItem("canteen_token");
  const headers = {
    "Content-Type": "application/json",
    ...(options.headers || {}),
  };
  if (token) headers.Authorization = `Bearer ${token}`;

  const response = await fetch(`${API_BASE}${path}`, {
    method: options.method || "GET",
    headers,
    body: options.body ? JSON.stringify(options.body) : undefined,
  });

  const result = await response.json();
  if (result.code === 4001) {
    localStorage.removeItem("canteen_token");
    localStorage.removeItem("canteen_user");
  }
  return result;
}

export const api = {
  register: (body) => request("/api/auth/register", { method: "POST", body }),
  login: (body) => request("/api/auth/login", { method: "POST", body }),
  me: () => request("/api/auth/me"),
  logout: () => request("/api/auth/logout", { method: "POST" }),
  changePassword: (body) => request("/api/auth/password", { method: "PUT", body }),
  updateProfile: (body) => request("/api/users/me/profile", { method: "PUT", body }),
  canteens: () => request("/api/canteens"),
  canteenDetail: (id) => request(`/api/canteens/${id}`),
  categories: () => request("/api/categories"),
  tags: () => request("/api/tags"),
  stalls: (params) => request(`/api/stalls?${new URLSearchParams(params).toString()}`),
  stallDetail: (id) => request(`/api/stalls/${id}`),
  stallReviews: (id, params = {}) => request(`/api/stalls/${id}/reviews?${new URLSearchParams(params).toString()}`),
  submitReview: (body) => request("/api/reviews", { method: "POST", body }),
  scoreRank: (params = {}) => request(`/api/rankings/score?${new URLSearchParams(params).toString()}`),
  hotRank: (params = {}) => request(`/api/rankings/hot?${new URLSearchParams(params).toString()}`),
  latestRank: (params = {}) => request(`/api/rankings/latest?${new URLSearchParams(params).toString()}`),
  myReviews: (params = {}) => request(`/api/users/me/reviews?${new URLSearchParams(params).toString()}`),
  updateMyReview: (id, body) => request(`/api/users/me/reviews/${id}`, { method: "PUT", body }),
  deleteMyReview: (id) => request(`/api/users/me/reviews/${id}`, { method: "DELETE" }),
  addFavorite: (body) => request("/api/users/me/favorites", { method: "POST", body }),
  deleteFavorite: (stallId) => request(`/api/users/me/favorites/${stallId}`, { method: "DELETE" }),
  favorites: (params = {}) => request(`/api/users/me/favorites?${new URLSearchParams(params).toString()}`),
  addBlacklist: (body) => request("/api/users/me/blacklist", { method: "POST", body }),
  deleteBlacklist: (stallId) => request(`/api/users/me/blacklist/${stallId}`, { method: "DELETE" }),
  blacklist: (params = {}) => request(`/api/users/me/blacklist?${new URLSearchParams(params).toString()}`),
  addHistory: (body) => request("/api/users/me/history", { method: "POST", body }),
  history: (params = {}) => request(`/api/users/me/history?${new URLSearchParams(params).toString()}`),
  recommendToday: (params = {}) => request(`/api/recommendations/today?${new URLSearchParams(params).toString()}`),
  recommendPersonalized: (body) => request("/api/recommendations/personalized", { method: "POST", body }),
  recommendFeed: (body) => request("/api/recommendations/feed", { method: "POST", body }),
  adminDeleteReview: (id) => request(`/api/admin/reviews/${id}`, { method: "DELETE" }),
  adminCreateStall: (body) => request("/api/admin/stalls", { method: "POST", body }),
  adminUpdateStall: (id, body) => request(`/api/admin/stalls/${id}`, { method: "PUT", body }),
  adminDeleteStall: (id) => request(`/api/admin/stalls/${id}`, { method: "DELETE" }),
  adminCreateCanteen: (body) => request("/api/admin/canteens", { method: "POST", body }),
  adminUpdateCanteen: (id, body) => request(`/api/admin/canteens/${id}`, { method: "PUT", body }),
  adminUsers: () => request("/api/admin/users"),
  adminUpdateUserRole: (id, body) => request(`/api/admin/users/${id}/role`, { method: "PUT", body }),
  adminTags: () => request("/api/admin/tags"),
  adminCreateTag: (body) => request("/api/admin/tags", { method: "POST", body }),
  adminUpdateTag: (id, body) => request(`/api/admin/tags/${id}`, { method: "PUT", body }),
};

