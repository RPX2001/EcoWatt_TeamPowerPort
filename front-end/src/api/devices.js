import api from './axios';

// Device Management APIs

export const getDevices = () => {
  return api.get('/devices');
};

export const getDevice = (deviceId) => {
  return api.get(`/devices/${deviceId}`);
};

export const registerDevice = (deviceData) => {
  return api.post('/devices', deviceData);
};

export const updateDevice = (deviceId, deviceData) => {
  return api.put(`/devices/${deviceId}`, deviceData);
};
