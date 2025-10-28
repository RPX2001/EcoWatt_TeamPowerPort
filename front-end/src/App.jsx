import { ThemeProvider } from '@mui/material/styles';
import CssBaseline from '@mui/material/CssBaseline';
import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import { BrowserRouter } from 'react-router-dom';
import theme from './theme/theme';
import Box from '@mui/material/Box';
import Typography from '@mui/material/Typography';
import Container from '@mui/material/Container';
import Paper from '@mui/material/Paper';

// Create a client
const queryClient = new QueryClient({
  defaultOptions: {
    queries: {
      refetchOnWindowFocus: false,
      retry: 1,
    },
  },
});

function App() {
  return (
    <QueryClientProvider client={queryClient}>
      <ThemeProvider theme={theme}>
        <CssBaseline />
        <BrowserRouter>
          <Box sx={{ minHeight: '100vh', bgcolor: 'background.default', py: 4 }}>
            <Container maxWidth="lg">
              <Paper elevation={3} sx={{ p: 4, textAlign: 'center' }}>
                <Typography variant="h3" component="h1" gutterBottom color="primary">
                  ⚡ EcoWatt Control Panel
                </Typography>
                <Typography variant="h6" color="text.secondary" gutterBottom>
                  Real-time Device Monitoring & Management
                </Typography>
                <Typography variant="body1" sx={{ mt: 3 }}>
                  Frontend setup complete! 🎉
                </Typography>
                <Typography variant="body2" color="text.secondary" sx={{ mt: 1 }}>
                  Start building the dashboard components...
                </Typography>
              </Paper>
            </Container>
          </Box>
        </BrowserRouter>
      </ThemeProvider>
    </QueryClientProvider>
  );
}

export default App;

