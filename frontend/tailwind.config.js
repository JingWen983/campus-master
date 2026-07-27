/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './admin.html', './teacher.html', './student.html', './parent.html', './src/**/*.{vue,ts,js}'],
  theme: {
    extend: {
      fontFamily: {
        sans: ['"Noto Sans SC"', 'sans-serif'],
        display: ['"Fraunces"', 'serif'],
      },
      colors: {
        ink: '#1a1410',
        cream: '#faf6ef',
      },
    },
  },
  plugins: [],
}
