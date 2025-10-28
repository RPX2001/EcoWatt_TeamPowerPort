import { useState } from 'react';
import {
  Box,
  Button,
  Card,
  CardContent,
  Typography,
  Alert,
  CircularProgress,
  Chip,
  Divider,
} from '@mui/material';
import CheckCircleIcon from '@mui/icons-material/CheckCircle';
import ErrorIcon from '@mui/icons-material/Error';
import { getDevices } from '../../api/devices';
import { getLatestData } from '../../api/aggregation';

const APITester = () => {
  const [testing, setTesting] = useState(false);
  const [results, setResults] = useState([]);

  const runTests = async () => {
    setTesting(true);
    setResults([]);
    const testResults = [];

    // Test 1: Get Devices
    try {
      const response = await getDevices();
      console.log('Full response:', response);
      console.log('Response data:', response?.data);
      console.log('Response status:', response?.status);
      
      // Handle case where response or response.data is undefined
      if (!response) {
        throw new Error('No response received from server');
      }
      
      if (!response.data) {
        throw new Error('Response has no data property');
      }
      
      const devices = response.data.devices || [];
      testResults.push({
        name: 'GET /devices',
        status: 'success',
        message: `Found ${devices.length} device(s)`,
        data: response.data,
      });
    } catch (error) {
      console.error('Devices error:', error);
      console.error('Error response:', error.response);
      testResults.push({
        name: 'GET /devices',
        status: 'error',
        message: error.response?.data?.error || error.message || 'Unknown error occurred',
      });
    }

    // Test 2: Get Latest Data (if we have devices)
    const firstTestData = testResults[0]?.data;
    if (testResults[0]?.status === 'success' && firstTestData?.devices?.length > 0) {
      try {
        const deviceId = firstTestData.devices[0].device_id;
        const response = await getLatestData(deviceId);
        testResults.push({
          name: `GET /aggregation/latest/${deviceId}`,
          status: 'success',
          message: 'Successfully fetched latest data',
          data: response.data,
        });
      } catch (error) {
        testResults.push({
          name: 'GET /aggregation/latest/:deviceId',
          status: 'error',
          message: error.response?.data?.error || error.message || 'Unknown error occurred',
        });
      }
    }

    setResults(testResults);
    setTesting(false);
  };

  return (
    <Box sx={{ mt: 3 }}>
      <Card elevation={3}>
        <CardContent>
          <Typography variant="h6" gutterBottom>
            API Connection Tester
          </Typography>
          <Typography variant="body2" color="text.secondary" sx={{ mb: 2 }}>
            Test connectivity with the Flask backend
          </Typography>

          <Button
            variant="contained"
            onClick={runTests}
            disabled={testing}
            startIcon={testing ? <CircularProgress size={20} /> : null}
          >
            {testing ? 'Testing...' : 'Run API Tests'}
          </Button>

          {results.length > 0 && (
            <Box sx={{ mt: 3 }}>
              <Divider sx={{ mb: 2 }} />
              <Typography variant="subtitle2" gutterBottom>
                Test Results:
              </Typography>
              {results.map((result, index) => (
                <Alert
                  key={index}
                  severity={result.status === 'success' ? 'success' : 'error'}
                  icon={result.status === 'success' ? <CheckCircleIcon /> : <ErrorIcon />}
                  sx={{ mb: 2 }}
                >
                  <Box>
                    <Typography variant="body2" sx={{ fontWeight: 600 }}>
                      {result.name}
                    </Typography>
                    <Typography variant="body2" sx={{ mt: 0.5 }}>
                      {result.message}
                    </Typography>
                    {result.data && (
                      <Box sx={{ mt: 1 }}>
                        <Chip
                          label="View Data"
                          size="small"
                          onClick={() => console.log(result.data)}
                          sx={{ cursor: 'pointer' }}
                        />
                      </Box>
                    )}
                  </Box>
                </Alert>
              ))}
            </Box>
          )}
        </CardContent>
      </Card>
    </Box>
  );
};

export default APITester;
