/**
 * KeyGenerator Component
 * 
 * Tool for generating cryptographic keys
 * Features:
 * - Key type selection (PSK/HMAC)
 * - Format selection (C header/PEM/binary)
 * - Key size configuration
 * - Copy to clipboard
 * - Download as file
 */

import React, { useState } from 'react';
import {
  Box,
  Paper,
  Typography,
  Button,
  FormControl,
  InputLabel,
  Select,
  MenuItem,
  TextField,
  Alert,
  CircularProgress,
  Divider,
  Stack,
  IconButton,
  Tooltip
} from '@mui/material';
import {
  VpnKey as KeyIcon,
  ContentCopy as CopyIcon,
  Download as DownloadIcon,
  Refresh as RegenerateIcon
} from '@mui/icons-material';
import { useMutation } from '@tanstack/react-query';
import { generateKeys } from '../../api/utilities';

const KeyGenerator = () => {
  const [keyType, setKeyType] = useState('PSK');
  const [format, setFormat] = useState('c_header');
  const [keySize, setKeySize] = useState(32);
  const [generatedKeys, setGeneratedKeys] = useState(null);
  const [copySuccess, setCopySuccess] = useState(false);

  const keyTypes = [
    { value: 'PSK', label: 'Pre-Shared Key (PSK)' },
    { value: 'HMAC', label: 'HMAC Key' },
    { value: 'AES', label: 'AES Key' }
  ];

  const formats = [
    { value: 'c_header', label: 'C Header (.h)' },
    { value: 'pem', label: 'PEM Format' },
    { value: 'hex', label: 'Hexadecimal' },
    { value: 'base64', label: 'Base64' }
  ];

  const keySizes = [
    { value: 16, label: '128-bit (16 bytes)' },
    { value: 24, label: '192-bit (24 bytes)' },
    { value: 32, label: '256-bit (32 bytes)' },
    { value: 64, label: '512-bit (64 bytes)' }
  ];

  const generateMutation = useMutation({
    mutationFn: () => generateKeys(keyType, format, keySize),
    onSuccess: (response) => {
      setGeneratedKeys(response.data);
      setCopySuccess(false);
    }
  });

  const handleGenerate = () => {
    generateMutation.mutate();
  };

  const handleCopy = (content) => {
    navigator.clipboard.writeText(content).then(() => {
      setCopySuccess(true);
      setTimeout(() => setCopySuccess(false), 2000);
    });
  };

  const handleDownload = (filename, content) => {
    const blob = new Blob([content], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = filename;
    link.click();
    URL.revokeObjectURL(url);
  };

  const handleDownloadAll = () => {
    if (!generatedKeys || !generatedKeys.keys) return;

    Object.entries(generatedKeys.keys).forEach(([filename, content]) => {
      handleDownload(filename, content);
    });
  };

  return (
    <Box>
      <Paper sx={{ p: 3 }}>
        <Typography variant="h6" gutterBottom>
          Cryptographic Key Generator
        </Typography>
        <Typography variant="body2" color="text.secondary" gutterBottom>
          Generate secure cryptographic keys for device authentication and encryption
        </Typography>

        <Divider sx={{ my: 3 }} />

        {/* Key Type Selection */}
        <Box sx={{ mb: 3 }}>
          <Typography variant="subtitle2" gutterBottom>
            1. Select Key Type
          </Typography>
          <FormControl fullWidth sx={{ mt: 1 }}>
            <InputLabel>Key Type</InputLabel>
            <Select
              value={keyType}
              label="Key Type"
              onChange={(e) => setKeyType(e.target.value)}
            >
              {keyTypes.map((type) => (
                <MenuItem key={type.value} value={type.value}>
                  {type.label}
                </MenuItem>
              ))}
            </Select>
          </FormControl>
        </Box>

        {/* Format Selection */}
        <Box sx={{ mb: 3 }}>
          <Typography variant="subtitle2" gutterBottom>
            2. Select Output Format
          </Typography>
          <FormControl fullWidth sx={{ mt: 1 }}>
            <InputLabel>Format</InputLabel>
            <Select
              value={format}
              label="Format"
              onChange={(e) => setFormat(e.target.value)}
            >
              {formats.map((fmt) => (
                <MenuItem key={fmt.value} value={fmt.value}>
                  {fmt.label}
                </MenuItem>
              ))}
            </Select>
          </FormControl>
        </Box>

        {/* Key Size Selection */}
        <Box sx={{ mb: 3 }}>
          <Typography variant="subtitle2" gutterBottom>
            3. Select Key Size
          </Typography>
          <FormControl fullWidth sx={{ mt: 1 }}>
            <InputLabel>Key Size</InputLabel>
            <Select
              value={keySize}
              label="Key Size"
              onChange={(e) => setKeySize(e.target.value)}
            >
              {keySizes.map((size) => (
                <MenuItem key={size.value} value={size.value}>
                  {size.label}
                </MenuItem>
              ))}
            </Select>
          </FormControl>
        </Box>

        {/* Generate Button */}
        <Stack direction="row" spacing={2} sx={{ mb: 3 }}>
          <Button
            variant="contained"
            color="primary"
            onClick={handleGenerate}
            disabled={generateMutation.isPending}
            startIcon={generateMutation.isPending ? <CircularProgress size={20} /> : <KeyIcon />}
            fullWidth
          >
            {generateMutation.isPending ? 'Generating...' : 'Generate Keys'}
          </Button>
          {generatedKeys && (
            <Button
              variant="outlined"
              onClick={handleDownloadAll}
              startIcon={<DownloadIcon />}
            >
              Download All
            </Button>
          )}
        </Stack>

        {/* Copy Success Alert */}
        {copySuccess && (
          <Alert severity="success" sx={{ mb: 2 }}>
            Copied to clipboard!
          </Alert>
        )}

        {/* Error Display */}
        {generateMutation.isError && (
          <Alert severity="error" sx={{ mb: 2 }}>
            {generateMutation.error?.response?.data?.error || 'Failed to generate keys'}
          </Alert>
        )}

        {/* Success Display */}
        {generateMutation.isSuccess && generatedKeys && (
          <Alert severity="success" sx={{ mb: 2 }}>
            {generatedKeys.message || 'Keys generated successfully!'}
          </Alert>
        )}

        {/* Generated Keys Display */}
        {generatedKeys && generatedKeys.keys && (
          <Box>
            <Divider sx={{ my: 3 }} />
            <Typography variant="h6" gutterBottom>
              Generated Keys
            </Typography>

            {Object.entries(generatedKeys.keys).map(([filename, content]) => (
              <Box key={filename} sx={{ mb: 3 }}>
                <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 1 }}>
                  <Typography variant="subtitle2" fontWeight="bold">
                    {filename}
                  </Typography>
                  <Stack direction="row" spacing={1}>
                    <Tooltip title="Copy to clipboard">
                      <IconButton
                        size="small"
                        onClick={() => handleCopy(content)}
                        color="primary"
                      >
                        <CopyIcon fontSize="small" />
                      </IconButton>
                    </Tooltip>
                    <Tooltip title="Download file">
                      <IconButton
                        size="small"
                        onClick={() => handleDownload(filename, content)}
                        color="primary"
                      >
                        <DownloadIcon fontSize="small" />
                      </IconButton>
                    </Tooltip>
                  </Stack>
                </Box>
                <Paper
                  sx={{
                    p: 2,
                    bgcolor: 'grey.100',
                    fontFamily: 'monospace',
                    fontSize: '0.875rem',
                    overflow: 'auto',
                    maxHeight: 300
                  }}
                >
                  <pre style={{ margin: 0, whiteSpace: 'pre-wrap', wordBreak: 'break-all' }}>
                    {content}
                  </pre>
                </Paper>
              </Box>
            ))}

            {/* Metadata */}
            {generatedKeys.timestamp && (
              <Typography variant="caption" color="text.secondary">
                Generated at: {new Date(generatedKeys.timestamp).toLocaleString()}
              </Typography>
            )}
          </Box>
        )}

        {/* Instructions */}
        <Box sx={{ mt: 4, p: 2, bgcolor: 'warning.lighter', borderRadius: 1 }}>
          <Typography variant="subtitle2" gutterBottom>
            ⚠️ Security Notice:
          </Typography>
          <Typography variant="body2" component="div">
            <ul style={{ margin: 0, paddingLeft: '1.5rem' }}>
              <li>Store generated keys securely</li>
              <li>Never commit keys to version control</li>
              <li>Use different keys for development and production</li>
              <li>Rotate keys regularly following security best practices</li>
              <li>Keep private keys confidential and share only public keys</li>
            </ul>
          </Typography>
        </Box>

        {/* Usage Instructions */}
        <Box sx={{ mt: 2, p: 2, bgcolor: 'info.lighter', borderRadius: 1 }}>
          <Typography variant="subtitle2" gutterBottom>
            ℹ️ How to Use:
          </Typography>
          <Typography variant="body2" component="div">
            <ul style={{ margin: 0, paddingLeft: '1.5rem' }}>
              <li>Select the key type based on your security requirements</li>
              <li>Choose C Header format for embedded systems (ESP32)</li>
              <li>256-bit (32 bytes) recommended for strong security</li>
              <li>Copy keys to your project or download as files</li>
              <li>Update both server and device with matching keys</li>
            </ul>
          </Typography>
        </Box>
      </Paper>
    </Box>
  );
};

export default KeyGenerator;
