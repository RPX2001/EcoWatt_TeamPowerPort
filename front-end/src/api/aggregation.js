import api from './axios';

// Aggregated Data APIs

export const getLatestData = async (deviceId) => {
  const response = await api.get(`/aggregated/${deviceId}`);
  return response.data;
};

export const getHistoricalData = async (deviceId, params = {}) => {
  const response = await api.get(`/aggregated/${deviceId}/history`, { params });
  return response.data;
};

export const uploadAggregatedData = async (deviceId, data) => {
  const response = await api.post(`/aggregated/${deviceId}`, data);
  return response.data;
};
