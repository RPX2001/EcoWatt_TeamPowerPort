import { io } from 'socket.io-client';

const SOCKET_URL = import.meta.env.DEV ? 'http://localhost:5001' : '';

let socket = null;

export const connectWebSocket = () => {
  if (!socket) {
    socket = io(SOCKET_URL, {
      transports: ['websocket'],
      reconnection: true,
      reconnectionDelay: 1000,
      reconnectionAttempts: 5,
    });

    socket.on('connect', () => {
      console.log('🟢 WebSocket connected');
    });

    socket.on('disconnect', () => {
      console.log('🔴 WebSocket disconnected');
    });

    socket.on('connect_error', (error) => {
      console.error('❌ WebSocket connection error:', error);
    });

    socket.on('reconnect', (attemptNumber) => {
      console.log(`🔄 WebSocket reconnected after ${attemptNumber} attempts`);
    });
  }

  return socket;
};

export const disconnectWebSocket = () => {
  if (socket) {
    socket.disconnect();
    socket = null;
    console.log('🔴 WebSocket manually disconnected');
  }
};

export const getSocket = () => socket;

export const subscribeToDevice = (deviceId) => {
  if (socket) {
    socket.emit('subscribe_device', { device_id: deviceId });
    console.log(`📡 Subscribed to device: ${deviceId}`);
  }
};

export const unsubscribeFromDevice = (deviceId) => {
  if (socket) {
    socket.emit('unsubscribe_device', { device_id: deviceId });
    console.log(`📡 Unsubscribed from device: ${deviceId}`);
  }
};
