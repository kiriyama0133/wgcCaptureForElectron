const addon = require('./build/capture_addon.node');

console.log('模块加载成功！');

try {
    // 既然对象里有 start，直接试试它
    const result = addon.start(); 
    console.log('Start 结果:', result);
    
    const size = addon.getSize();
    console.log(`显示器尺寸: ${size.width} x ${size.height}`);
} catch (err) {
    console.error('调用失败:', err);
}