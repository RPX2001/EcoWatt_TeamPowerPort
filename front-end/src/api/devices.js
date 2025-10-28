import api from './axios';

// Device Management APIs

export const getDevices = async () => {
  const response = await api.get('/devices');
  return response.data;
};

export const getDevice = async (deviceId) => {
  const response = await api.get(`/devices/${deviceId}`);
  return response.data;
};

export const registerDevice = async (deviceData) => {
  const response = await api.post('/devices', deviceData);
  return response.data;
};

export const updateDevice = async (deviceId, deviceData) => {
  const response = await api.put(`/devices/${deviceId}`, deviceData);
  return response.data;
};
