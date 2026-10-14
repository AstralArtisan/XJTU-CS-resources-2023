export const state = {
  token: localStorage.getItem("canteen_token") || "",
  user: JSON.parse(localStorage.getItem("canteen_user") || "null"),
};

export function setSession(token, user) {
  state.token = token;
  state.user = user;
  if (token) localStorage.setItem("canteen_token", token);
  if (user) localStorage.setItem("canteen_user", JSON.stringify(user));
}

export function clearSession() {
  state.token = "";
  state.user = null;
  localStorage.removeItem("canteen_token");
  localStorage.removeItem("canteen_user");
}
