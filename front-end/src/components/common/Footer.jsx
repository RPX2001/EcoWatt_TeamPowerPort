import { Box, Container, Typography, Link } from '@mui/material';

const Footer = () => {
  return (
    <Box
      component="footer"
      sx={{
        py: 3,
        px: 2,
        mt: 'auto',
        backgroundColor: (theme) =>
          theme.palette.mode === 'light'
            ? theme.palette.grey[200]
            : theme.palette.grey[800],
      }}
    >
      <Container maxWidth="lg">
        <Typography variant="body2" color="text.secondary" align="center">
          {'EcoWatt Control Panel © '}
          {new Date().getFullYear()}
          {' | Built with '}
          <Link color="inherit" href="https://mui.com/" target="_blank" rel="noopener">
            Material-UI
          </Link>
          {' & '}
          <Link color="inherit" href="https://vitejs.dev/" target="_blank" rel="noopener">
            Vite
          </Link>
        </Typography>
        <Typography variant="caption" color="text.secondary" align="center" display="block" sx={{ mt: 1 }}>
          Team PowerPort | EN4440 Project
        </Typography>
      </Container>
    </Box>
  );
};

export default Footer;
